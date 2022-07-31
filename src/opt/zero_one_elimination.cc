#include "ir/ir.hpp"
#include "opt/opt.h"
#include "json.hpp"
#include <queue>
#include <stack>
#include <cstdio>
#include <cstring>

namespace opt {


namespace zero_one_elimination {

using GVT = std::shared_ptr<ir::Value>;

void Zero_One_Elimination(ir::Module &m, std::unique_ptr<ir::Func> &func) {
    //若是，直接返回
    if (func->is_declaration())
        return;

    for (auto &bb : func->bblocks) {
        std::vector<ir::Instr *> wait_to_delete;
//auto instr = bb->insts.begin();instr != bb->insts.end();instr++
        for (auto instr = bb->insts.begin();instr != bb->insts.end();instr++) {
            auto &use_instr = *instr;
            if (use_instr->is_add()) {
                //进行类型转化
                auto BinaryAlu_instr = dynamic_cast<ir::BinaryAlu *>(use_instr.get());
                //获取两个操作数
                auto lhs = dynamic_cast<ir::ImmValue *>(BinaryAlu_instr->l.get());
                auto rhs = dynamic_cast<ir::ImmValue *>(BinaryAlu_instr->r.get());
              
                //这里的判断语句不是很确定
                //use_instr->ReplaceValue();
                
                if (lhs && lhs->imm == 0) {
                    BinaryAlu_instr->result = BinaryAlu_instr->r;
                    
                   // instr->replace_all_use_with(rhs);
                    wait_to_delete.push_back(BinaryAlu_instr);
                } else if (rhs && rhs->imm == 0) {
                    BinaryAlu_instr->result = BinaryAlu_instr->l;
                    wait_to_delete.push_back(BinaryAlu_instr);
                }
            } else if (use_instr->is_sub()) {
                //进行类型转化
                auto BinaryAlu_instr = dynamic_cast<ir::BinaryAlu *>(use_instr.get());
                //获取两个操作数
                auto lhs = dynamic_cast<ir::ImmValue *>(BinaryAlu_instr->l.get());
                auto rhs = dynamic_cast<ir::ImmValue *>(BinaryAlu_instr->r.get());

                if (rhs && rhs->imm == 0) {
                    BinaryAlu_instr->result = BinaryAlu_instr->l;
                    wait_to_delete.push_back(BinaryAlu_instr);
                }
            } else if (use_instr->is_mul()) {
                 //进行类型转化
                auto BinaryAlu_instr = dynamic_cast<ir::BinaryAlu *>(use_instr.get());
                //获取两个操作数
                auto lhs = dynamic_cast<ir::ImmValue *>(BinaryAlu_instr->l.get());
                auto rhs = dynamic_cast<ir::ImmValue *>(BinaryAlu_instr->r.get());

                if (lhs && lhs->imm == 0) {
                    BinaryAlu_instr->result = 0;
                    wait_to_delete.push_back(BinaryAlu_instr);

                } else if (lhs && lhs->imm == 1) {
                    BinaryAlu_instr->result = BinaryAlu_instr->r;
                    wait_to_delete.push_back(BinaryAlu_instr);
                    // instr->replace_all_use_with(rhs);
                    // wait_delete.push_back(instr);
                } else if (rhs && rhs->imm == 0) {
                    BinaryAlu_instr->result = 0;
                    //instr->replace_all_use_with(ConstantInt::get(0, m_));
                    wait_to_delete.push_back(BinaryAlu_instr);
                } else if (rhs && rhs->imm == 1) {
                    BinaryAlu_instr->result = BinaryAlu_instr->l;
                    //instr->replace_all_use_with(lhs);
                    wait_to_delete.push_back(BinaryAlu_instr);
                }
            } else if (use_instr->is_div()) {
                //进行类型转化
                auto BinaryAlu_instr = dynamic_cast<ir::BinaryAlu *>(use_instr.get());
                //获取两个操作数
                auto lhs = dynamic_cast<ir::ImmValue *>(BinaryAlu_instr->l.get());
                auto rhs = dynamic_cast<ir::ImmValue *>(BinaryAlu_instr->r.get());
                if (rhs && rhs->imm == 1) {
                    BinaryAlu_instr->result = BinaryAlu_instr->l;
                    //instr->replace_all_use_with(lhs);
                    wait_to_delete.push_back(BinaryAlu_instr);
                }
            }
        }
        for (auto instr_to_delete : wait_to_delete) {
            for(auto bb_inst = bb->insts.begin();bb_inst != bb->insts.end();bb_inst++){
                if( instr_to_delete == bb_inst->get())
                    bb->insts.erase(bb_inst);
            }
            
        }
    }

   
}

} // namespace zero_one_elimination

void Zero_One_Elimination(ir::Module &m) {
    for (auto &func : m.funcs) {
        zero_one_elimination::Zero_One_Elimination(m, func);
    }
}

} // namespace opt