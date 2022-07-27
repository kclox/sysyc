#include "ir/ir.hpp"
#include "opt/opt.h"
#include "json.hpp"
#include <queue>
#include <stack>
#include <cstdio>
#include <cstring>

namespace opt {

namespace active_variable_analysis {

using GVT = std::shared_ptr<ir::Value>;

void Active_Variable_Analysis(ir::Module &m, std::unique_ptr<ir::Func> &func) {

    m.live_in.clear();
    m.live_out.clear();
    m.use.clear();
    m.def.clear();

    if (func->bblocks.empty()) {
        return;
    } else {
        // 在此分析 func 的每个bb块的活跃变量，并存储在 live_in live_out 结构内
       
        for (auto &bb : func->bblocks) {
            for (auto instr = bb->insts.begin();instr != bb->insts.end();instr++) {
                auto &use_instr = *instr;
                if (use_instr->is_add() || use_instr->is_sub() || use_instr->is_mul() ||
                    use_instr->is_div() || use_instr->is_rem()) {                    
                    auto BinaryAlu_instr = dynamic_cast<ir::BinaryAlu *>(use_instr.get());
                    auto lhs = dynamic_cast<ir::ImmValue *>(BinaryAlu_instr->l.get());
                    auto rhs = dynamic_cast<ir::ImmValue *>(BinaryAlu_instr->r.get());
                    //auto constant_ptr = dynamic_cast<ConstantInt *>(lval);
                    if (!lhs) {
                        if (m.def[&*bb].find(lhs) == m.def[&*bb].end())
                            m.use[&*bb].insert(lhs);
                        for (auto prevbb : bb->predecessors) {
                            m.active[prevbb].insert(lhs);
                        }
                    }
                    //auto constant_ptr = dynamic_cast<ConstantInt *>(rval);
                    if (!rhs) {
                        if (m.def[&*bb].find(rhs) == m.def[&*bb].end())
                            m.use[&*bb].insert(rhs);
                        for (auto prevbb : bb->predecessors) {
                            m.active[prevbb].insert(lhs);
                        }
                    }
                    m.def[&*bb].insert(&*BinaryAlu_instr->result);
                } else if (use_instr->is_ret()) {
                    //进行类型转化
                    auto Return_instr = dynamic_cast<ir::Ret *>(use_instr.get());
                    //如果有返回值
                    if (Return_instr->retval) { 
                         auto temp = Return_instr->retval;
                        // auto constant_fp_ptr = dynamic_cast<ConstantFP *>(tmp);
                        // auto constant_int_ptr =
                        //     dynamic_cast<ConstantInt *>(tmp);
                        
                            if (m.def[&*bb].find(&*temp) == m.def[&*bb].end()) {
                                m.use[&*bb].insert(&*temp);
                                for (auto prevbb : bb->predecessors) {
                                    m.active[prevbb].insert(&*temp);
                                }
                            }
                        
                    }
                    m.def[&*bb].insert(&*Return_instr->retval);
                } else if (use_instr->is_cmp()) {
                    //进行类型转化
                    auto Compare_instr = dynamic_cast<ir::Icmp *>(use_instr.get());
                    auto lhs = dynamic_cast<ir::ImmValue *>(Compare_instr->l.get());
                    auto rhs = dynamic_cast<ir::ImmValue *>(Compare_instr->r.get());
                    //不存在左操作数
                    if (!lhs) {
                        if (m.def[&*bb].find(lhs) == m.def[&*bb].end())
                            m.use[&*bb].insert(lhs);
                        for (auto prevbb : bb->predecessors) {
                            m.active[prevbb].insert(lhs);
                        }
                    }
                    //不存在右操作数
                    if (!rhs) {
                        if (m.def[&*bb].find(rhs) == m.def[&*bb].end())
                            m.use[&*bb].insert(rhs);
                        for (auto prevbb : bb->predecessors) {
                            m.active[prevbb].insert(rhs);
                        }
                    }
                    m.def[&*bb].insert(&*Compare_instr->result);                 
                } else if (use_instr->is_zext()) {
                    //进行类型转化
                    auto Zest_instr = dynamic_cast<ir::Zext *>(use_instr.get());

                    auto temp = Zest_instr->val;
                    if (m.def[&*bb].find(&*temp) == m.def[&*bb].end()) {
                        m.use[&*bb].insert(&*temp);
                        for (auto prevbb : bb->predecessors) {
                            m.active[prevbb].insert(&*temp);
                        }
                    }
                    m.def[&*bb].insert(&*Zest_instr->result);                 
                } else if (use_instr->is_call()) {
                    //进行类型转化
                    auto Call_instr = dynamic_cast<ir::Call *>(use_instr.get());
                    for (int i = 1; i < Call_instr->args.size(); i++) {
                        auto temp = Call_instr->args[i];
                        if (!temp) {
                            if (m.def[&*bb].find(&*temp) == m.def[&*bb].end()) {
                                m.use[&*bb].insert(&*temp);
                                for (auto prevbb : bb->predecessors) {
                                    m.active[prevbb].insert(&*temp);
                                }
                            }
                        }
                    }
                    m.def[&*bb].insert(&*Call_instr->result);
                } else if (use_instr->is_phi()) {
                    //进行类型转化
                    auto Phi_instr = dynamic_cast<ir::Phi *>(use_instr.get());
                    for (int i = 0; i < Phi_instr->vals.size(); i += 2) {
                        auto temp = Phi_instr->vals[i];
                        if (!temp.val) {
                            if (m.def[&*bb].find(&*temp.val) == m.def[&*bb].end()) {
                                m.use[&*bb].insert(&*temp.val);
                                for (auto prevbb : bb->predecessors) {
                                    if (prevbb->label == Phi_instr->vals[i + 1].label)
                                        m.active[prevbb].insert(&*temp.val);
                                }
                            }
                        }
                    }
                    m.def[&*bb].insert(&*Phi_instr->result);                   
                } else if (use_instr->is_load()) {
                    //进行类型转化
                    auto Load_instr = dynamic_cast<ir::Load *>(use_instr.get());
                    auto temp = Load_instr->ptr;     
                    if (!temp) {
                        if (m.def[&*bb].find(&*temp) == m.def[&*bb].end()) {
                            m.use[&*bb].insert(&*temp);
                            for (auto prevbb : bb->predecessors) {
                                m.active[prevbb].insert(&*temp);
                            }
                        }
                    }
                    m.def[&*bb].insert(&*Load_instr->result);
                     //修改到这里22.7.27
                } else if (use_instr->is_gep()) {
                    //进行类型转化
                    auto Gep_instr = dynamic_cast<ir::Getelementptr *>(use_instr.get());
                    for (int i = 0; i < Gep_instr->indices.size(); i++) {
                        auto temp = Gep_instr->indices[i];

                        if (!temp) {
                            if (m.def[&*bb].find(&*temp) == m.def[&*bb].end()) {
                                m.use[&*bb].insert(&*temp);
                                for (auto prevbb : bb->predecessors) {
                                    m.active[prevbb].insert(&*temp);
                                }
                            }
                        }
                    }
                    m.def[&*bb].insert(&*Gep_instr->result);
                } else if (use_instr->is_alloca()) {
                    //进行类型转化
                    auto Alloca_instr = dynamic_cast<ir::Alloca *>(use_instr.get());
                    m.def[&*bb].insert(&*Alloca_instr->result);
                }
            }

        }
        // live_out.clear();
        bool flag = true;
        while (flag) {
            flag = false;
            for (auto &bb : func->bblocks) {
                for (auto succbb : bb->predecessors) {
                    for (auto it : m.live_in[succbb]) {
                        if (m.live_out[&*bb].find(it) == m.live_out[&*bb].end()) {
                            if (m.active[&*bb].find(it) != m.active[&*bb].end())
                                m.live_out[&*bb].insert(it);
                        }
                    }
                }
                for (auto it : m.use[&*bb]) {
                    if (m.live_in[&*bb].find(it) == m.live_in[&*bb].end()) {
                        flag = true;
                        m.live_in[&*bb].insert(it);
                    }
                }
                for (auto it : m.live_out[&*bb]) {
                    if (m.def[&*bb].find(it) == m.def[&*bb].end()) {
                        if (m.live_in[&*bb].find(it) == m.live_in[&*bb].end()) {
                            flag = true;
                            m.live_in[&*bb].insert(it);
                            for (auto prevbb : bb->predecessors) {
                                m.active[prevbb].insert(it);
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace active_variable_analysis

void Active_Variable_Analysis(ir::Module &m) {
    for (auto &func : m.funcs) {
        active_variable_analysis::Active_Variable_Analysis(m, func);
    }
}

} // namespace opt