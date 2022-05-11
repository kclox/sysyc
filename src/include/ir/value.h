#ifndef __ir_value_h__
#define __ir_value_h__

#include "ir/type.h"
#include <memory>
#include <string>
#include <vector>

namespace ir {

struct Value {
    int kind;
    enum { kImm, kLocalVar, kTmpVar, kGlobalVar };
    std::shared_ptr<Type> ty;
    Value() {}
    Value(int kind) : kind(kind) {}
    Value(std::shared_ptr<Type> ty, int kind) : ty(ty), kind(kind) {}
    virtual std::string str() = 0;
    virtual std::string typed_str() { return ty->str() + " " + str(); }
};

struct ImmValue : public Value {
    // i1, i32
    using Value::Value;
    int imm;
    ImmValue() : Value(kImm) {}
    ImmValue(std::shared_ptr<Type> ty, int imm) : Value(ty, kImm), imm(imm) {}
    virtual std::string str() { return std::to_string(imm); }

    static std::shared_ptr<ImmValue> i1_zero, i1_one;
};

struct LocalValue : public Value {
    // local value
    using Value::Value;
    virtual std::string var() = 0;
    virtual std::string str() { return "%" + var(); }
};

struct LocalVar : public LocalValue {
    // named local var
    LocalVar() : LocalValue(kLocalVar) {}
    LocalVar(std::shared_ptr<Type> ty, std::string name)
        : LocalValue(ty, kLocalVar), name(name) {}
    std::string name;
    virtual std::string var() { return name; }
};

struct TmpVar : public LocalValue {
    // tmp var
    int id;
    TmpVar() : LocalValue(kTmpVar) {}
    TmpVar(std::shared_ptr<Type> ty, int id = 0)
        : LocalValue(ty, kTmpVar), id(id) {}
    virtual ~TmpVar() {}
    virtual std::string var() { return std::to_string(id); }
};

struct GlobalVar : public Value {
    // global var
    std::string name;
    GlobalVar() : Value(kGlobalVar) {}
    GlobalVar(std::shared_ptr<Type> ty, std::string name)
        : Value(ty, kGlobalVar), name(name) {}
    virtual std::string str() { return "@" + name; }

    virtual std::string typed_str() {
        if (ty->kind == Type::kFunction) {
            auto ft = ty->cast<FuncT>();
            return ft->ret->str() + " " + str();
        } else {
            return Value::typed_str();
        }
    }
};

} // namespace ir

#endif