#include "ir/ir.hpp"
#include "ir/type.h"

namespace ir {

std::shared_ptr<Type> t_undef = std::make_shared<Type>(Type::kUndef),
                      t_void = std::make_shared<Type>(Type::kVoid),
                      t_i32 = std::make_shared<Type>(Type::kI32),
                      t_i1 = std::make_shared<Type>(Type::kI1),
                      t_label = std::make_shared<Type>(Type::kLabel);
std::shared_ptr<PtrT> t_pi32 = std::make_shared<PtrT>(t_i32);

std::shared_ptr<ImmValue> ImmValue::i1_zero =
    std::make_shared<ir::ImmValue>(t_i1, 0),
                          ImmValue::i1_one =
                              std::make_shared<ir::ImmValue>(t_i1, 1);

std::string Define::str() {
    std::string s = var->str() + " = ";
    if (is_const)
        s += "constant ";
    else
        s += "global ";
    s += init->str();
    return s;
}

void BB::dump(std::ostream &os) {
    auto get_tag = [&]() {
        if (tag == kDefault)
            return std::string("");
        static std::string tag_m[] = {"", "header", "body", "exit", "ret"};
        return "; tag: " + tag_m[tag];
    };
    if (label) {
        os << label->var() << ":\t" << get_tag() << "\n";
    }
    for (auto &inst : insts) {
        os << "  " << inst->str() << "\n";
    }
}

std::shared_ptr<Value> Func::FindVar(std::string var, bool only_in_func) {
    auto p = vars[var];
    if (p) {
        return p;
    }
    if (only_in_func)
        return {};
    return m->FindVar(var);
}

void Func::dump(std::ostream &os) {
    ResetTmpVar();
    os << "define " << ft->ret->str() << " "
       << "@" + name << "(";
    foreach (
        args, FOREACH_IDX_LAMBDA() { os << args[i]->typed_str(); },
        FOREACH_IDX_LAMBDA() { os << ", "; })
        ;
    os << ") {\n";
    for (auto &bb : bblocks) {
        bb->dump(os);
    }
    os << "}\n";
}

void Module::dump(std::ostream &os) {
    // dump global variable/const declarations
    for (auto &def : defs) {
        os << def->str() << "\n";
    }
    os << "\n";

    // dump function declarations
    for (auto &decl : func_decls) {
        os << decl->str() << "\n";
    }
    os << "\n";

    // dump functions
    for (auto &f : funcs) {
        f->dump(os);
        os << "\n";
    }
}

std::string TypedValues(std::shared_ptr<Type> ty,
                        std::vector<std::shared_ptr<Value>> vals) {
    std::string s = ty->str() + " ";
    foreach (
        vals, FOREACH_LAMBDA() { s += (*it)->str(); },
        FOREACH_LAMBDA() { s += ", "; })
        ;
    return s;
}

} // namespace ir
