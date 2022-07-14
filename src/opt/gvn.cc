#include "ir/ir.hpp"
#include "opt/opt.h"

namespace opt {

using GVT = std::shared_ptr<ir::Value>;

namespace GVN {

std::string myhash(ir::Instr *i) {
    // 简单的哈希算法，使用 OpName + LeftOpName + RightOpName作为哈希值
    auto ret = std::to_string(i->op);
    
    if (ir::Instr::kOpAlloca ||i->op == ir::Instr::kOpLoad ||i->op ==  ir::Instr::kOpCall ||!i->HasResult()) {
        return ret + i->str() ;
        //是否就是i->str()?
    }
    ret += std::to_string(i->op);
    if (auto cmp = dynamic_cast<ir::Icmp *>(i)) {
        ret += "C" + std::to_string(cmp->cond) + "M";
    }
    //
    for (auto op: std::to_string(i->op)) {
         if (auto is = dynamic_cast<ir::Instr *>(i)) {
            ret += is->str();
        } else if (auto arg = dynamic_cast<ir::Func *>(i)){
            ret += arg->name;
        } else if (auto bb = dynamic_cast<ir::BB *>(i)){
            ret += "BB" + bb->id;
        } else {
            assert(0 && "Unsupported");
        }
    }
    return ret;
}

// 
void globalValueNumbering(ir::Module &m, std::unique_ptr<ir::Func> &func) {
    
    func->ResetBBID(); // 初始化所有指令编号
    bool changed;

    std::map<std::string, std::set<ir::Instr *>> _map;
    

    do {
        _map.clear();
        changed = false;
        // 遍历所有BB
        //to be deleted
        std::set<ir::Instr *> tbd;
        for (auto &bb: func->bblocks) {
            for (auto it_inst = bb->insts.begin(); it_inst != bb->insts.end();it_inst++) {
                // 如果有返回值且不是call/load则计算哈希值
                auto &inst = *it_inst;
                if (inst->kOpAlloca ||inst->op == ir::Instr::kOpLoad ||inst->op ==  ir::Instr::kOpCall ||!inst->HasResult()) {
                    continue;
                }
                //这里的转化存在一定疑惑
                auto inst_hash = myhash(&*inst);
                //测试的时候用于输出
                // std::cout <<inst_hash <<std::endl;
                //这里需要定义_map;
                
                

                if (_map.find(inst_hash) != _map.end()) {
                    bool replaced = false;
                    for (auto ii: _map[inst_hash]) {
                        // 检查是否可以进行替换
                        //改到这里
                        //if (Idom->isDominatedBy(inst->get_parent(), ii->get_parent())) 
                        std::map<GVT, GVT> c_var_map;
                        if (func->isDominatedBy(&*bb, ii->get_parent())) {
                            // 后者被前者支配，直接进行替换
                            replaced = true;
                            //
                            inst->ReplaceValues(c_var_map);
                            //
                            tbd.insert(&*inst);
                            break;
                        } else if (func->isDominatedBy(ii->get_parent(), &*bb)) {
                            // 由于遍历顺序问题，反向替换
                            replaced = true;
                            //
                            ii->ReplaceValues(c_var_map);
                            //
                            tbd.insert(ii);
                            _map[inst_hash].erase(ii);
                            _map[inst_hash].insert(&*inst);
                            break;
                        }
                    }
                    if (!replaced) {
                        _map[inst_hash].insert(&*inst);
                    } else {
                        changed = true;
                    }
                } else {
                    // 直接加入
                    _map[inst_hash].insert(&*inst);
                }
            }
        }
        // 删除所有被替换的指令
        for (auto inst: tbd) {
            //能否直接释放空间?
            delete inst;

        }
    } while (changed);
}


} // namespace GVN

void gvn(ir::Module &m) {

    for (auto &func: m.funcs) {
        //是declaration则continue
        //if (func->bblocks ) continue;
        //已替换
       GVN::globalValueNumbering(m, func);
    }

}

}// namespace opt