#ifndef __ir_type_h__
#define __ir_type_h__

#include "utils.h"
#include <memory>
#include <string>
#include <vector>

namespace ir {

struct Type {
    enum { kUndef, kVoid, kI32, kI1, kLabel, kPtr, kArray, kFunction, kStruct };
    int kind;

    Type() {}
    Type(int kind) : kind(kind) {}

    virtual std::string str() {
        static std::string m[] = {"undef", "void", "i32", "i1", "label"};
        return m[kind];
    }

    virtual bool eq(const Type &other) const { return other.kind == kind; }

    virtual int size() const {
        static int size_map[] = {0, 0, 4, 1, 0};
        return size_map[kind];
    }

    template <typename T> T *cast() const { return (T *)(this); }
};

struct PtrT : public Type {
    std::shared_ptr<Type> p;
    explicit PtrT() : Type(kPtr) {}
    explicit PtrT(std::shared_ptr<Type> p) : PtrT() { this->p = p; }

    virtual std::string str() { return p->str() + "*"; }

    virtual bool eq(const Type &other) const {
        return Type::eq(other) && p->eq(*other.cast<PtrT>()->p);
    }

    virtual int size() const { return 4; }
};

struct ArrayT : public Type {
    int count;
    std::shared_ptr<Type> element;

    ArrayT() : Type(kArray) {}
    ArrayT(std::shared_ptr<Type> element, int count) : ArrayT() {
        this->element = element;
        this->count = count;
    }

    virtual std::string str() {
        return "[" + std::to_string(count) + " x " + element->str() + "]";
    }

    virtual bool eq(const Type &other) const {
        return Type::eq(other) && element->eq(*other.cast<ArrayT>()->element);
    }

    virtual int size() const { return count * element->size(); }
};

struct FuncT : public Type {
    std::shared_ptr<Type> ret;
    std::vector<std::shared_ptr<Type>> args;

    FuncT() : Type(kFunction) {}

    FuncT(std::shared_ptr<Type> ret,
          std::initializer_list<std::shared_ptr<Type>> args)
        : FuncT() {
        this->ret = ret;
        this->args = args;
    }

    virtual std::string str() {
        std::string s = ret->str() + " (";
        foreach (
            args, [&](auto it) { s += (*it)->str(); },
            [&](auto it) { s += ", "; })
            ;
        s += ")";
        return s;
    }

    virtual bool eq(const Type &other) const {
        if (!Type::eq(other))
            return false;
        auto ft = other.cast<FuncT>();
        if (!ret->eq(*ft->ret))
            return false;
        if (args.size() != ft->args.size())
            return false;
        for (int i = 0; i < args.size(); i++) {
            if (!args[i]->eq(*ft->args[i]))
                return false;
        }
        return true;
    }
};

struct StructT : Type {
    std::vector<std::shared_ptr<Type>> fileds;

    StructT() : Type(kStruct) {}

    virtual std::string str() {
        std::string s = "<{";
        foreach (
            fileds, [&](auto it) { s += (*it)->str(); },
            [&](auto it) { s += ", "; })
            ;
        s += "}>";
        return s;
    }

    virtual bool eq(const Type &other) const {
        if (!Type::eq(other))
            return false;
        auto st = other.cast<StructT>();
        if (fileds.size() != st->fileds.size())
            return false;
        for (int i = 0; i < fileds.size(); i++) {
            if (!fileds[i]->eq(*st->fileds[i]))
                return false;
        }
        return true;
    }

    virtual int size() const {
        int sz = 0;
        for (auto filed : fileds)
            sz += filed->size();
        return sz;
    }
};

extern std::shared_ptr<Type> t_undef, t_void, t_i32, t_i1, t_label;
extern std::shared_ptr<PtrT> t_pi32;

} // namespace ir

#endif