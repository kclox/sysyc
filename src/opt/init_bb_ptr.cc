#include "ir/ir.hpp"
#include "opt/opt.h"

namespace opt {
void InitBBPtr(ir::Module &m, std::unique_ptr<ir::Func> &func) {
    for (auto &bb : func->bblocks) {
        auto &inst = bb->insts.back();
        if (inst->op == ir::Instr::kOpBr) {
            auto *br = dynamic_cast<ir::Br *>(inst.get());
            auto to_bb1 = func->label_map[br->l1.get()];
            to_bb1->predecessors.push_back(bb.get());
            bb->successors.insert(to_bb1);
            if (br->l2) {
                auto to_bb2 = func->label_map[br->l2.get()];
                to_bb2->predecessors.push_back(bb.get());
                bb->successors.insert(to_bb2);
            }
        }
    }
}

void InitBBPtr(ir::Module &m) {
    for (auto &func : m.funcs) {
        InitBBPtr(m, func);
    }
}

} // namespace opt