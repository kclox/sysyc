#include "ir/ir.hpp"
#include "opt/opt.h"
#include "json.hpp"
#include <queue>
#include <stack>
#include <cstdio>
#include <cstring>

namespace opt {

namespace dead_code_elimination {

using GVT = std::shared_ptr<ir::Value>;

static bool isSafeToDelete(ir::Module &m,ir::Instr *i) {
    //||!i->get_use_list().empty() 该条件因为暂时不清楚如何设置被删除 应该与findandreplace函数相关
    if (i->is_store() || i->is_call() || i->is_br() || i->is_ret() ) return false;
   
    return true;
}

void Dead_Code_Elimination(ir::Module &m, std::unique_ptr<ir::Func> &func) {

       
        if (func->is_declaration()) return;
        for (auto &bb: func->bblocks) {
            std::vector<ir::Instr *> tbd;
            do {
                tbd.clear();
                for (auto inst = bb->insts.begin();inst != bb->insts.end();inst++) {
                    if (isSafeToDelete(m,inst->get())) {
                        //|| inst->get_use_list().empty() 该条件因为暂时不清楚如何设置被删除 该与findandreplace函数相关
                        if (!m.isLiveOut(inst->get())) {
                            tbd.push_back(inst->get());
                        }
                    }
                }
                for (auto tbd_inst : tbd) {
                    for(auto bb_inst = bb->insts.begin();bb_inst != bb->insts.end();bb_inst++){
                if( tbd_inst == bb_inst->get())
                    bb->insts.erase(bb_inst);
            }
                        

                }
            } while (!tbd.empty());
        }
    

}

} // namespace dead_code_elimination

void Dead_Code_Elimination(ir::Module &m) {
    for (auto &func : m.funcs) {
        dead_code_elimination::Dead_Code_Elimination(m, func);
    }
    
}

} // namespace opt