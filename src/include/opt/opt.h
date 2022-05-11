#ifndef __opt_opt_h__
#define __opt_opt_h__

#include "ir/ir.hpp"
#include <functional>

namespace opt {
extern int log_level;

namespace dtree {
struct Node {
    std::vector<ir::BB *> children;
    ir::BB *parent;

    Node(ir::BB *parent = nullptr) : parent(parent) {}
};

Node &GetNode(ir::BB *bb);
ir::BB *IDom(ir::BB *bb);
} // namespace dtree

void ConstantOpt(ir::Module &m);
void InitBBPtr(ir::Module &m);
void BuildDTree(ir::Module &m);
void Mem2Reg(ir::Module &m);

namespace mgr {
typedef void (*OptModuleEntry)(ir::Module &);

struct OptModule {
    std::string name;
    OptModuleEntry entry;
    std::vector<OptModule *> dependencies;
    bool done;

    OptModule(std::string name, OptModuleEntry entry,
              std::vector<OptModuleEntry> dependencies);

    void RunOpt(ir::Module &m);
};

extern const int module_count;
extern OptModule modules[];

bool RunOpt(std::string name, ir::Module &m);
} // namespace mgr

} // namespace opt

#endif
