#ifndef __ast_type_h__
#define __ast_type_h__

#include "utils.h"
#include "json.hpp"
#include <assert.h>
#include <map>
#include <vector>

namespace ast {

using json_t = nlohmann::json;

struct Type {
    int kind;
    enum { kNudef, kInt, kVoid, kArray, kFunc };

    Type() { kind = kNudef; }
    Type(int k) : kind(k) {}

    virtual operator std::string() const {
        static const char *type_str[] = {"undef", "int", "void"};
        assert(kind < sizeof(type_str) / sizeof(const char *));
        return type_str[kind];
    }

    virtual operator json_t() const { return to_json(); }
    virtual json_t to_json() const { return json_t((std::string) * this); }
};

struct ArrayT : public Type {
    Type *element;
    std::vector<int> dimensions;
    ArrayT() : Type(kArray) {}
    ArrayT(Type *ele) : Type(kArray) { element = ele; }

    int Count() {
        int sum = 0;
        bool is_first = true;
        for (auto x : dimensions) {
            if (is_first)
                sum = x;
            else
                sum *= x;
            is_first = false;
        }
        return sum;
    }

    int ChildCount() {
        if (dimensions.size() <= 1)
            return 1;
        int sum = 1;
        for (auto it = dimensions.begin() + 1; it != dimensions.end(); it++)
            sum *= *it;
        return sum;
    }

    virtual operator std::string() const {
        std::string ret = *element;
        for (auto i : dimensions) {
            ret += "[" + std::to_string(i) + "]";
        }
        return ret;
    }

    virtual json_t to_json() const {
        return json_t{{"element", (json_t)*element}, {"count", dimensions}};
    }
};

struct FuncT : public Type {
    Type *ret;
    std::vector<Type *> args;

    FuncT() : Type(kFunc) {}

    virtual operator std::string() const {
        std::string result = (std::string)*ret + "(";
        bool first = true;
        for (auto x : args) {
            if (!first)
                result += ", ";
            else
                first = false;
            result += (std::string)*x;
        }
        result += ")";
        return result;
    }

    virtual json_t to_json() const {
        json_t j{{"ret", (std::string)*ret}};
        j["args"] = {};
        for (auto x : args) {
            j["args"].push_back((std::string)*x);
        }
        return j;
    }
};

class TypeSystem {
  private:
    std::vector<ArrayT *> int_array_types;
    Type t_undef = {Type::kNudef}, t_int = {Type::kInt}, t_void = {Type::kVoid};

  public:
    TypeSystem() {}

    virtual ~TypeSystem() {
        for (auto x : int_array_types) {
            delete x;
        }
    }

    Type *TUndef() { return &t_undef; }
    Type *TInt() { return &t_int; }
    Type *TVoid() { return &t_void; }

    ArrayT *TArray(Type *ele) {
        auto t = new ArrayT(ele);
        int_array_types.push_back(t);
        return t;
    }

    ArrayT *TArray(Type *ele, std::initializer_list<int> dims) {
        auto t = new ArrayT(ele);
        int_array_types.push_back(t);
        t->dimensions = dims;
        return t;
    }
};

extern TypeSystem g_type_system;

}; // namespace ast

#endif
