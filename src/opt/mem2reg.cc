#include "ir/ir.hpp"
#include "opt/opt.h"
#include "json.hpp"
#include <queue>
#include <stack>
#include <cstdio>
#include <cstring>

using json_t = nlohmann::json;

namespace opt {

namespace mem2reg {

bool dbg_df = false;

using GVT = std::shared_ptr<ir::Value>;

void ComputeDominanceFrontier(std::unique_ptr<ir::Func> &func,
                              std::vector<std::set<ir::BB *>> &df_list) {
    df_list.resize(func->bblocks.size());

    for (int i = 0; i < func->bblocks.size(); i++) {
        auto &b = func->bblocks[i];
        if (b->predecessors.size() >= 2) {
            for (auto &p : b->predecessors) {
                auto runner = p;
                while (runner != dtree::IDom(b.get())) {
                    df_list[runner->id].insert(b.get());
                    runner = dtree::IDom(runner);
                }
            }
        }
    }

    if (dbg_df) {
        for (int i = 0; i < df_list.size(); i++) {
            fprintf(stderr, "%d: ", func->bblocks[i]->id);
            for (auto df : df_list[i]) {
                fprintf(stderr, "%d ", df->id);
            }
            fprintf(stderr, "\n");
        }
    }
}

struct RenameHelper {
    std::vector<std::unique_ptr<ir::BB>> &bblocks;
    std::set<GVT> &variables;
    std::map<ir::Value *, std::stack<GVT>> stack;

    RenameHelper(std::vector<std::unique_ptr<ir::BB>> &bblocks,
                 std::set<GVT> &variables)
        : bblocks(bblocks), variables(variables) {}

    GVT GetCurrentValue(ir::Value *v) {
        auto &_stack = stack[v];
        if (_stack.size() <= 0)
            return std::make_shared<ir::ImmValue>(
                std::dynamic_pointer_cast<ir::PtrT>(v->ty)->p, 0);
        else
            return _stack.top();
    }

    void Rename(ir::BB *bb) {
        std::vector<std::pair<GVT, GVT>> to_replace;
        auto replace = [&](std::unique_ptr<ir::Instr> &inst) {
            for (auto x : to_replace)
                inst->ReplaceValue(x.first, x.second);
        };
        std::map<ir::Value *, int> push_count;
        for (auto it_inst = bb->insts.begin(); it_inst != bb->insts.end();
             it_inst++) {
            auto &inst = *it_inst;
            if (inst->op == ir::Instr::kOpPhi) {
                auto phi = dynamic_cast<ir::Phi *>(inst.get());
                auto v = phi->result.get();
                // check [1] NOTE
                auto res = std::make_shared<ir::TmpVar>(phi->ty);
                stack[v].push(res), push_count[v]++;
                phi->result = res;
                continue;
            }
            replace(inst);
            if (inst->op == ir::Instr::kOpStore) {
                // store ty val, ty* pa
                // [pa].push(val)
                auto store = dynamic_cast<ir::Store *>(inst.get());
                if (variables.count(store->ptr) > 0) {
                    stack[store->ptr.get()].push(store->val),
                        push_count[store->ptr.get()]++;
                    bb->insts.erase(it_inst--);
                }
            } else if (inst->op == ir::Instr::kOpLoad) {
                // res = load ty, ty* pa
                auto load = dynamic_cast<ir::Load *>(inst.get());
                if (variables.count(load->ptr) > 0) {
                    to_replace.push_back(
                        {load->result, GetCurrentValue(load->ptr.get())});
                    bb->insts.erase(it_inst--);
                }
            }
        }

        for (auto &s : bb->successors) {
            for (auto &inst : s->insts) {
                if (inst->op != ir::Instr::kOpPhi)
                    break;
                auto phi = dynamic_cast<ir::Phi *>(inst.get());
                for (auto &p : phi->vals) {
                    if (p.label == bb->label) {
                        // NOTE: p.val is one ptr which point to variable to
                        // rename
                        p.val = GetCurrentValue(p.val.get());
                    }
                }
            }
        }

        for (auto &b : dtree::GetNode(bb).children) {
            Rename(b);
        }

        for (auto [v, count] : push_count) {
            while (count-- > 0)
                stack[v].pop();
        }
    }
};

void Rename(std::unique_ptr<ir::Func> &func, std::set<GVT> &variables) {
    RenameHelper h(func->bblocks, variables);
    h.Rename(func->bblocks[0].get());
}

void InsertPhi(ir::BB *bb, GVT v) {
    auto phi = std::make_unique<ir::Phi>();
    phi->ty = std::dynamic_pointer_cast<ir::PtrT>(v->ty)->p;
    // [1] NOTE: mark result as variable to rename, and set it when rename
    phi->result = v; // std::make_shared<ir::TmpVar>(phi->ty);
    for (auto &pred : bb->predecessors) {
        // NOTE: val is one ptr which point to variable to rename
        phi->vals.push_back(ir::Phi::PhiVal{v, pred->label});
    }
    bb->insts.insert(bb->insts.begin(), std::move(phi));
}

void Mem2Reg(ir::Module &m, std::unique_ptr<ir::Func> &func) {
    std::vector<std::set<ir::BB *>> DF;
    ComputeDominanceFrontier(func, DF);

    int bb_count = func->bblocks.size();
    std::set<ir::Value *> orig[bb_count];
    std::map<ir::Value *, std::set<ir::BB *>> defsites;
    std::map<ir::Value *, std::set<ir::BB *>> PHI;

    // get variables
    std::set<GVT> variables;
    auto &bb0 = func->bblocks.front();
    for (auto it_inst = bb0->insts.begin(); it_inst != bb0->insts.end();
         it_inst++) {
        auto &inst = *it_inst;
        // ty* pa = alloca ty
        if (inst->op == ir::Instr::kOpAlloca) {
            auto var = inst->Result();
            auto ptr_ty = std::dynamic_pointer_cast<ir::PtrT>(var->ty);
            if (ptr_ty->p->kind != ir::Type::kArray) {
                variables.insert(var);
                bb0->insts.erase(it_inst--);
            }
        }
    }

    for (auto &bb : func->bblocks) {
        for (auto &inst : bb->insts) {
            // dectect var def
            if (inst->op == ir::Instr::kOpStore) {
                // store ty val, ty* pa -> val -> a_i
                auto store = dynamic_cast<ir::Store *>(inst.get());
                if (variables.count(store->ptr) > 0) {
                    auto v = store->ptr.get();
                    orig[bb->id].insert(v);
                    defsites[v].insert(bb.get());
                }
            }
        }
    }

    for (auto &v : variables) {
        // set of bb, which defines v
        auto W = defsites[v.get()];
        while (W.size() > 0) {
            auto _it_n = W.begin();
            auto n = *_it_n;
            W.erase(_it_n);
            for (auto y : DF[n->id]) {
                if (PHI[v.get()].count(y) <= 0) {
                    InsertPhi(y, v);
                    PHI[v.get()].insert(y);
                    if (orig[y->id].count(v.get()) <= 0) {
                        W.insert(y);
                    }
                }
            }
        }
    }

    Rename(func, variables);
    func->ResetTmpVar();
}

} // namespace mem2reg

void Mem2Reg(ir::Module &m) {
    for (auto &func : m.funcs) {
        mem2reg::Mem2Reg(m, func);
    }
}

} // namespace opt