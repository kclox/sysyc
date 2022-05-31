#ifndef __ir_init_h__
#define __ir_init_h__

#include "ir/type.h"
#include "ir/value.h"
#include "utils.h"
#include <string>
#include <vector>
#include <cassert>

namespace ir {

struct InitVal {
    enum { kUndef, kZero, kBasic, kArray, kStruct };
    int kind;
    InitVal(int kind) : kind(kind) {}
    virtual std::string str() const = 0;
    virtual std::string str_arm() const = 0;

    virtual std::shared_ptr<Type> type() = 0;
    virtual bool IsType(const Type &ty) const = 0;
};

struct Zeroinitializer : public InitVal {
    std::shared_ptr<Type> ty;

    Zeroinitializer() : InitVal(kZero) {}
    Zeroinitializer(std::shared_ptr<Type> ty) : InitVal(kZero), ty(ty) {}

    virtual std::string str() const { return ty->str() + " zeroinitializer"; }
    virtual std::string str_arm() const {
        std::string pre = ".word 0";
        return pre;
    }
    virtual std::shared_ptr<Type> type() { return ty; }
    virtual bool IsType(const Type &ty) const { return this->ty->eq(ty); }
};

struct BasicInit : public InitVal {
    // std::shared_ptr<Type> ty;
    std::shared_ptr<Value> val;

    BasicInit() : InitVal(kBasic) {}
    BasicInit(std::shared_ptr<Value> val) : InitVal(kBasic), val(val) {}

    virtual std::string str() const { return val->typed_str(); }
    virtual std::string str_arm() const {
        std::string pre = ".word ";
        return pre + val->str();
    }
    virtual std::shared_ptr<Type> type() { return val->ty; }
    virtual bool IsType(const Type &ty) const { return this->val->ty->eq(ty); }
};

struct ArrayInit : public InitVal {
    std::shared_ptr<ArrayT> ty;
    std::vector<std::unique_ptr<InitVal>> vals;

    ArrayInit() : InitVal(kArray) {}

    virtual std::string str() const {
        auto valstr = [&](InitVal *val) -> std::string {
            if (val)
                return val->str();
            else
                return ty->element->str() + " 0";
        };
        std::string result;
        if (vals.size() > 0) {
            result = ty->str();
            result += " [";
            for (int i = 0; i + 1 < vals.size(); i++) {
                result += valstr(vals[i].get()) + ", ";
            }
            result += valstr(vals.back().get()) + "]";
        } else {
            assert(false && "array init can not be empty");
        }
        return result;
    }

    virtual std::string str_arm() const {
        std::string pre = ".word";
        auto valstr = [&](InitVal *val) -> std::string {
            if (val)
                return val->str_arm();
            else
                return pre + " 0";
        };
        std::string result;
        if (vals.size() > 0) {
            for (int i = 0; i + 1 < vals.size(); i++) {
                result += valstr(vals[i].get()) + "\n";
            }
            result += valstr(vals.back().get()) ;
        } else {
            assert(false && "array init can not be empty");
        }
        return result;
    }
    virtual std::shared_ptr<Type> type() { return ty; }
    virtual bool IsType(const Type &ty) const { return this->ty->eq(ty); }
};

struct StructInit : public InitVal {
    std::shared_ptr<StructT> st;
    std::vector<std::unique_ptr<InitVal>> vals;

    StructInit() : InitVal(kStruct) {}

    virtual std::string str() const {
        std::string result;
        if (vals.size() > 0) {
            result = st->str() + " <{";
            for (int i = 0; i + 1 < vals.size(); i++) {
                result += vals[i]->str() + ", ";
            }
            result += vals.back()->str() + "}>";
        } else {
            assert(false && "struct init can not be empty");
        }
        return result;
    }
    virtual std::string str_arm() const = 0;

    virtual std::shared_ptr<Type> type() { return st; }
    virtual bool IsType(const Type &ty) const { return st->eq(ty); }
};

} // namespace ir

#endif
