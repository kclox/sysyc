/**
 * Constant Folding (Constant-Expression Evaluation) & Constant Propagation
 * EG:
 *  before:
 *      %1 = add i32 1, 2
 *      %2 = add i32 %1, 3
 *      %b = add i32 %a, %2
 *  after:
 *      %b = add i32 %a, 6
 *
 *
 */

#include "ir/ir.hpp"
#include "opt/opt.h"

namespace opt {

using GVT = std::shared_ptr<ir::Value>;

static std::shared_ptr<ir::ImmValue>
CreateImm(ir::Module &m, std::shared_ptr<ir::Type> &ty, int val) {
    if (ty->kind == ir::Type::kI32) {
        return m.CreateImm(val);
    } else {
        return std::make_shared<ir::ImmValue>(ty, val);
    }
}

static inline int ImmVal(GVT &v) {
    return std::dynamic_pointer_cast<ir::ImmValue>(v)->imm;
}

int EvalBinaryAlu(ir::BinaryAlu *alu) {
    int l = ImmVal(alu->l), r = ImmVal(alu->r);
    switch (alu->op) {
    case ir::Instr::kOpAdd:
        return l + r;
        break;
    case ir::Instr::kOpSub:
        return l - r;
        break;
    case ir::Instr::kOpMul:
        return l * r;
        break;
    case ir::Instr::kOpSdiv:
        return l / r;
        break;
    case ir::Instr::kOpSrem:
        return l % r;
        break;
    case ir::Instr::kOpAnd:
        return l & r;
        break;
    case ir::Instr::kOpOr:
        return l | r;
        break;
    default:
        assert(false && "BUG");
    }
    return 0;
}

bool EvalIcmp(ir::Icmp *icmp) {
    int l = ImmVal(icmp->l), r = ImmVal(icmp->r);
    switch (icmp->cond) {
    case ir::Icmp::kEq:
        return l == r;
    case ir::Icmp::kNe:
        return l != r;
    case ir::Icmp::kUgt:
        return (unsigned)l > (unsigned)r;
    case ir::Icmp::kUge:
        return (unsigned)l >= (unsigned)r;
    case ir::Icmp::kUlt:
        return (unsigned)l < (unsigned)r;
    case ir::Icmp::kUle:
        return (unsigned)l <= (unsigned)r;
    case ir::Icmp::kSgt:
        return l > r;
    case ir::Icmp::kSge:
        return l >= r;
    case ir::Icmp::kSlt:
        return l < r;
    case ir::Icmp::kSle:
        return l <= r;
    default:
        assert(false);
    }
    return false;
}

static inline bool IsImm(GVT &v) { return v->kind == ir::Value::kImm; }

/**
 * Constant Propagation and Constant Folding
 *  NOTE: do not handle constant array like a[0] + b[1]
 */
void ConstantOpt(ir::Module &m) {
    // collect all constant variables
    std::map<GVT, GVT> g_constants;
    for (auto it_def = m.defs.begin(); it_def != m.defs.end(); it_def++) {
        auto &def = *it_def;
        if (def->is_const) {
            if (def->init->kind == ir::InitVal::kBasic) {
                auto init = dynamic_cast<ir::BasicInit *>(def->init.get());
                g_constants.insert({def->var, init->val});
                m.defs.erase(it_def--);
            }
        }
    }

    for (auto &func : m.funcs) {
        std::map<GVT, GVT> c_var_map;

        for (auto &bb : func->bblocks) {
            for (auto it_inst = bb->insts.begin(); it_inst != bb->insts.end();
                 it_inst++) {
                auto &inst = *it_inst;
                inst->ReplaceValues(c_var_map);
                GVT res;
                bool can_folding = false;
                if (inst->IsBinaryAlu()) {
                    auto alu = dynamic_cast<ir::BinaryAlu *>(inst.get());
                    can_folding = IsImm(alu->l) && IsImm(alu->r);
                    if (can_folding)
                        res = m.CreateImm(EvalBinaryAlu(alu), alu->ty);
                } else if (inst->op == ir::Instr::kOpIcmp) {
                    auto icmp = dynamic_cast<ir::Icmp *>(inst.get());
                    can_folding = IsImm(icmp->l) && IsImm(icmp->r);
                    if (can_folding)
                        res = m.CreateImm(EvalIcmp(icmp), ir::t_i1);
                } else if (inst->op == ir::Instr::kOpLoad) {
                    auto load = dynamic_cast<ir::Load *>(inst.get());
                    auto it = g_constants.find(load->ptr);
                    can_folding = it != g_constants.end();
                    if (can_folding)
                        res = it->second;
                }
                if (can_folding) {
                    c_var_map.insert({inst->Result(), res});
                    bb->insts.erase(it_inst--);
                }
            }
        }
        // replace var in phi
        for (auto &bb : func->bblocks) {
            for (auto &inst : bb->insts) {
                if (inst->op != ir::Instr::kOpPhi)
                    break;
                inst->ReplaceValues(c_var_map);
            }
        }
        // remove constant var
        for (auto [k, v] : c_var_map) {
            if (k->kind == ir::Value::kLocalVar) {
                func->vars.erase(
                    std::dynamic_pointer_cast<ir::LocalVar>(k)->var());
            }
        }
        // refresh tmp var id
        func->ResetTmpVar();
    }

    // delete consts
    for (auto &[k, v] : g_constants) {
        if (k->kind == ir::Value::kGlobalVar) {
            m.vars.erase(std::dynamic_pointer_cast<ir::GlobalVar>(k)->name);
        }
    }
}

} // namespace opt
