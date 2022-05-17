#include "opt/opt.h"
#include <string>

namespace opt {
int log_level = 0;

namespace mgr {

std::map<OptModuleEntry, OptModule *> module_entry_map;
std::map<std::string, OptModule *> module_name_map;

OptModule modules[] = {
    OptModule{"init-bb-ptr", InitBBPtr, {}},
    OptModule{"build-dtree", BuildDTree, {InitBBPtr}},
    OptModule{"mem2reg", Mem2Reg, {BuildDTree}},
    OptModule{"constant-opt", ConstantOpt, {}},
};
const int module_count = sizeof(modules) / sizeof(OptModule);

std::vector<std::string> levels[] = {
    {},
    {"mem2reg", "constant-opt"},
};
const int level_count =
    sizeof(levels) / sizeof(std::vector<std::string>);

OptModule::OptModule(std::string name, OptModuleEntry entry,
                     std::vector<OptModuleEntry> dependencies)
    : name(name), entry(entry) {
    module_entry_map.insert({entry, this});
    module_name_map.insert({name, this});
    for (auto dep : dependencies)
        this->dependencies.push_back(module_entry_map[dep]);
    done = false;
}

void OptModule::RunOpt(ir::Module &m) {
    if (done)
        return;
    for (auto dep : dependencies)
        dep->RunOpt(m);
    if (log_level >= 1)
        fprintf(stderr, "opt: %s\n", name.c_str());
    entry(m);
    done = true;
}

bool RunOpt(std::string name, ir::Module &m) {
    auto it = module_name_map.find(name);
    if (it != module_name_map.end()) {
        it->second->RunOpt(m);
        return true;
    }
    return false;
}

} // namespace mgr
} // namespace opt