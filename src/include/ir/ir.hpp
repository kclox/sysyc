#ifndef __ir_ir_hpp__
#define __ir_ir_hpp__

#include "ir/init.h"
#include "ir/type.h"
#include "ir/value.h"
#include "utils.h"
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <unordered_set>
#include <string>
#include <vector>

namespace ir {
struct BB;
struct Func;
struct Module;

typedef std::shared_ptr<Value> *PGVT;

std::string TypedValues(std::shared_ptr<Type> ty,
                        std::vector<std::shared_ptr<Value>> vals);




struct Instr {
    int op;
    //增加定位基本块
    BB *parent;
    enum {
        kOpCall = 1,
        kOpRet,
        kOpBr,
        kOpIcmp,
        kOpAdd,
        kOpSub,
        kOpMul,
        kOpSdiv,
        kOpSrem,
        kOpAnd,
        kOpOr,
        kOpLoad,
        kOpStore,
        kOpAlloca,
        kOpPhi,
        kOpGetelementptr,
        kOpZext,
        kOpBitcast,
    };

    Instr() {}
    Instr(int op) : op(op) {}
    //张启涵：根据基本块创建指令，主要用于定位基本块
    Instr(BB *parent):parent(parent){};
    inline const BB *get_parent() const { return parent; }
    inline BB *get_parent() { return parent; }
    ////////////////////////////////////
    bool IsBinaryAlu() const { return op >= kOpAdd && op <= kOpOr; }
    //张启涵:增加判断是否为相应的指令
    bool is_add() const { return op == kOpAdd; }
    bool is_sub() const { return op == kOpSub; }
    bool is_mul() const { return op == kOpMul; }
    bool is_div() const { return op == kOpSdiv; }
    bool is_rem() const { return op == kOpSrem; }
    bool is_ret() const { return op == kOpRet; }
    bool is_cmp() const { return op == kOpIcmp; }
    bool is_zext() const { return op == kOpZext; }
    bool is_call() const { return op == kOpCall; }
    bool is_phi() const { return op == kOpPhi; }
    bool is_load() const { return op == kOpLoad; }
    bool is_gep() const { return op == kOpGetelementptr; }
    bool is_alloca() const { return op == kOpAlloca;}
    ///////////////////////////////////
    virtual bool HasResult() const = 0;
    virtual std::shared_ptr<Value> Result() = 0;
    virtual int ReplaceValue(std::shared_ptr<Value> old,
                             std::shared_ptr<Value> _new) = 0;
    virtual int ReplaceValues(
        std::map<std::shared_ptr<Value>, std::shared_ptr<Value>> &m) = 0;
    virtual std::vector<PGVT> RValues() = 0;

    virtual std::string str() { return ""; }

    template <typename T>
    bool
    FindAndReplace(std::map<std::shared_ptr<Value>, std::shared_ptr<Value>> &m,
                   std::shared_ptr<T> &v) {
        auto it = m.find(v);
        if (it != m.end()) {
            v = std::dynamic_pointer_cast<T>(it->second);
            return true;
        }
        return false;
    }
};

struct Call : Instr {
    // <result> = call <ret_ty> <func>(<ty1> <arg1>, ...)
    std::shared_ptr<GlobalVar> func;
    std::vector<std::shared_ptr<Value>> args;
    std::shared_ptr<Value> result;

    Call() : Instr(kOpCall) {}

    inline FuncT *ft() { return func->ty->cast<FuncT>(); }

    virtual std::string str() {
        std::string call = "call " + func->typed_str() + "(";
        foreach (
            args, FOREACH_LAMBDA() { call += (*it)->typed_str(); },
            FOREACH_LAMBDA() { call += ", "; })
            ;
        call += ")";

        if (!ft()->ret->eq(*t_void) && result)
            call = result->str() + " = " + call;
        return call;
    }

    virtual bool HasResult() const { return result.operator bool(); }
    virtual std::shared_ptr<Value> Result() { return result; }
    virtual int ReplaceValue(std::shared_ptr<Value> old,
                             std::shared_ptr<Value> _new) {
        int res = 0;
        auto p = old.get();
        if (func.get() == p)
            func = std::dynamic_pointer_cast<GlobalVar>(_new), res++;
        if (result.get() == p)
            result = _new, res++;
        for (auto &arg : args)
            if (arg.get() == p)
                arg = _new, res++;
        return res;
    }

    virtual int
    ReplaceValues(std::map<std::shared_ptr<Value>, std::shared_ptr<Value>> &m) {
        int res = 0;
        res += FindAndReplace(m, func);
        res += FindAndReplace(m, result);
        for (auto &arg : args)
            res += FindAndReplace(m, arg);
        return res;
    }

    virtual std::vector<PGVT> RValues() {
        std::vector<PGVT> rvals;
        rvals.push_back((PGVT)&func);
        for (auto &x : args)
            rvals.push_back((PGVT)&x);
        return rvals;
    }
};

struct Ret : Instr {
    // ret <ty> [<val>]
    std::shared_ptr<Type> ty;
    std::shared_ptr<Value> retval;
    Ret() : Instr(kOpRet) {}
    Ret(std::shared_ptr<Type> ty, std::shared_ptr<Value> retval = {})
        : Instr(kOpRet), ty(ty), retval(retval) {}

    virtual std::string str() {
        std::string s = "ret " + ty->str();
        if (!ty->eq(*t_void) && retval)
            s += " " + retval->str();
        return s;
    }

    virtual bool HasResult() const { return false; }
    virtual std::shared_ptr<Value> Result() { return {}; }
    virtual int ReplaceValue(std::shared_ptr<Value> old,
                             std::shared_ptr<Value> _new) {
        auto p = old.get();
        int res = 0;
        if (retval && retval.get() == p)
            retval = _new, res++;
        return res;
    }

    virtual int
    ReplaceValues(std::map<std::shared_ptr<Value>, std::shared_ptr<Value>> &m) {
        int res = 0;
        if (retval)
            res += FindAndReplace(m, retval);
        return res;
    }

    virtual std::vector<PGVT> RValues() {
        if (retval)
            return {&retval};
        return {};
    }
};

struct Br : Instr {
    // br i1 <cond>, label <iftrue>, label <iffalse>
    // br i1 <cond>, label <dest>
    std::shared_ptr<Value> cond;
    // l1 for true block, l2 for false block
    std::shared_ptr<LocalValue> l1, l2;

    Br() : Instr(kOpBr), cond({}) {}
    Br(std::shared_ptr<LocalValue> l1, std::shared_ptr<Value> cond = {},
       std::shared_ptr<LocalValue> l2 = {})
        : Instr(kOpBr), l1(l1), cond(cond), l2(l2) {}

    virtual std::string str() {
        if (cond) {
            return StrFormat("br %s, %s, %s", cond->typed_str().c_str(),
                             l1->typed_str().c_str(), l2->typed_str().c_str());
        } else {
            return "br " + l1->typed_str();
        }
    }

    virtual bool HasResult() const { return false; }
    virtual std::shared_ptr<Value> Result() { return {}; }
    virtual int ReplaceValue(std::shared_ptr<Value> old,
                             std::shared_ptr<Value> _new) {
        auto p = old.get();
        int res = 0;
        if (l1.get() == p)
            l1 = std::dynamic_pointer_cast<LocalValue>(_new), res++;
        if (cond && cond.get() == p)
            cond = _new, res++;
        if (l2 && l2.get() == p)
            l2 = std::dynamic_pointer_cast<LocalValue>(_new), res++;
        return res;
    }

    virtual int
    ReplaceValues(std::map<std::shared_ptr<Value>, std::shared_ptr<Value>> &m) {
        int res = 0;
        res += FindAndReplace(m, l1);
        if (cond) {
            res += FindAndReplace(m, cond);
            res += FindAndReplace(m, l2);
        }
        return res;
    }

    virtual std::vector<PGVT> RValues() {
        return {&cond, (PGVT)&l1, (PGVT)&l2};
    }
};

struct Icmp : Instr {
    // icmp <cond> <ty> <l>, <r>
    int cond;
    std::shared_ptr<Type> ty;
    std::shared_ptr<Value> l, r;
    std::shared_ptr<Value> result;
    enum { kEq, kNe, kUgt, kUge, kUlt, kUle, kSgt, kSge, kSlt, kSle };
    Icmp() : Instr(kOpIcmp) {}
    Icmp(int cond, std::shared_ptr<Type> ty, std::shared_ptr<Value> l,
         std::shared_ptr<Value> r, std::shared_ptr<Value> result)
        : Instr(kOpIcmp), cond(cond), ty(ty), l(l), r(r), result(result) {}

    virtual std::string str() {
        static const char *cond_tb[] = {
            "eq", "ne", "ugt", "uge", "ult", "ule", "sgt", "sge", "slt", "sle",
        };
        return StrFormat("%s = icmp %s %s", result->str().c_str(),
                         cond_tb[cond], TypedValues(ty, {l, r}).c_str());
    }

    virtual bool HasResult() const { return result.operator bool(); }
    virtual std::shared_ptr<Value> Result() { return result; }
    virtual int ReplaceValue(std::shared_ptr<Value> old,
                             std::shared_ptr<Value> _new) {
        auto p = old.get();
        int res = 0;
        if (l.get() == p)
            l = _new, res++;
        if (r.get() == p)
            r = _new, res++;
        if (result.get() == p)
            result = _new, res++;
        return res;
    }

    virtual int
    ReplaceValues(std::map<std::shared_ptr<Value>, std::shared_ptr<Value>> &m) {
        int res = 0;
        res += FindAndReplace(m, l);
        res += FindAndReplace(m, r);
        res += FindAndReplace(m, result);
        return res;
    }

    virtual std::vector<PGVT> RValues() { return {&l, &r}; }
};

struct BinaryAlu : public Instr {
    std::shared_ptr<Type> ty;
    std::shared_ptr<Value> l, r;
    std::shared_ptr<Value> result;
    BinaryAlu() {}
    BinaryAlu(int op) : Instr(op) {}
    BinaryAlu(int op, std::shared_ptr<Type> ty, std::shared_ptr<Value> l,
              std::shared_ptr<Value> r, std::shared_ptr<Value> result)
        : Instr(op), ty(ty), l(l), r(r), result(result) {}

    virtual std::string str() {
        static std::map<int, std::string> m = {
            {ir::Instr::kOpAdd, "add"},   {ir::Instr::kOpSub, "sub"},
            {ir::Instr::kOpMul, "mul"},   {ir::Instr::kOpSdiv, "sdiv"},
            {ir::Instr::kOpSrem, "srem"}, {ir::Instr::kOpAnd, "and"},
            {ir::Instr::kOpOr, "or"}};
        return result->str() + " = " + m[op] + " " + TypedValues(ty, {l, r});
    }

    virtual bool HasResult() const { return result.operator bool(); }
    virtual std::shared_ptr<Value> Result() { return result; }
    virtual int ReplaceValue(std::shared_ptr<Value> old,
                             std::shared_ptr<Value> _new) {
        auto p = old.get();
        int res = 0;
        if (l.get() == p)
            l = _new, res++;
        if (r.get() == p)
            r = _new, res++;
        if (result.get() == p)
            result = _new, res++;
        return res;
    }

    virtual int
    ReplaceValues(std::map<std::shared_ptr<Value>, std::shared_ptr<Value>> &m) {
        int res = 0;
        res += FindAndReplace(m, l);
        res += FindAndReplace(m, r);
        res += FindAndReplace(m, result);
        return res;
    }

    virtual std::vector<PGVT> RValues() { return {&l, &r}; }
};

struct Load : Instr {
    // <result> = load <ty>, <typtr> <ptr>
    std::shared_ptr<Type> ty;
    std::shared_ptr<Value> ptr;
    std::shared_ptr<Value> result;
    int alignment;
    Load() : Instr(kOpLoad) {}
    Load(std::shared_ptr<Type> ty, std::shared_ptr<Value> ptr,
         std::shared_ptr<Value> result, int alignment = 4)
        : Instr(kOpLoad), ty(ty), ptr(ptr), result(result),
          alignment(alignment) {}

    virtual std::string str() {
        return result->str() + " = load " + ty->str() + ", " + ptr->typed_str();
    }

    virtual bool HasResult() const { return result.operator bool(); }
    virtual std::shared_ptr<Value> Result() { return result; }
    virtual int ReplaceValue(std::shared_ptr<Value> old,
                             std::shared_ptr<Value> _new) {
        auto p = old.get();
        int res = 0;
        if (ptr.get() == p)
            ptr = _new, res++;
        if (result.get() == p)
            result = _new, res++;
        return res;
    }

    virtual int
    ReplaceValues(std::map<std::shared_ptr<Value>, std::shared_ptr<Value>> &m) {
        int res = 0;
        res += FindAndReplace(m, ptr);
        res += FindAndReplace(m, result);
        return res;
    }

    virtual std::vector<PGVT> RValues() { return {(PGVT)&ptr}; }
};

struct Store : Instr {
    // store <ty> <value>, <typtr> <ptr>
    std::shared_ptr<Value> val;
    std::shared_ptr<Value> ptr;
    int alignment;
    Store() : Instr(kOpStore) {}
    Store(std::shared_ptr<Value> val, std::shared_ptr<Value> ptr,
          int alignment = 4)
        : Instr(kOpStore), val(val), ptr(ptr), alignment(alignment) {}

    virtual std::string str() {
        return "store " + val->typed_str() + ", " + ptr->typed_str();
    }

    virtual bool HasResult() const { return false; }
    virtual std::shared_ptr<Value> Result() { return {}; }
    virtual int ReplaceValue(std::shared_ptr<Value> old,
                             std::shared_ptr<Value> _new) {
        auto p = old.get();
        int res = 0;
        if (val.get() == p)
            val = _new, res++;
        if (ptr.get() == p)
            ptr = _new, res++;
        return res;
    }

    virtual int
    ReplaceValues(std::map<std::shared_ptr<Value>, std::shared_ptr<Value>> &m) {
        int res = 0;
        res += FindAndReplace(m, val);
        res += FindAndReplace(m, ptr);
        return res;
    }

    virtual std::vector<PGVT> RValues() { return {&val, (PGVT)&ptr}; }
};

struct Alloca : Instr {
    // alloca <ty> [, align <alignment>]
    std::shared_ptr<Type> ty;
    int alignment;
    std::shared_ptr<Value> result;
    Alloca() : Instr(kOpAlloca) {}
    Alloca(std::shared_ptr<Type> ty, std::shared_ptr<Value> result,
           int alignment = 4)
        : Instr(kOpAlloca), ty(ty), result(result), alignment(alignment) {}

    virtual std::string str() {
        // align ignored now
        return result->str() + " = alloca " + ty->str() + ", align " +
               std::to_string(alignment);
    }

    virtual bool HasResult() const { return result.operator bool(); }
    virtual std::shared_ptr<Value> Result() { return result; }
    virtual int ReplaceValue(std::shared_ptr<Value> old,
                             std::shared_ptr<Value> _new) {
        auto p = old.get();
        int res = 0;
        if (result.get() == p)
            result = _new, res++;
        return res;
    }

    virtual int
    ReplaceValues(std::map<std::shared_ptr<Value>, std::shared_ptr<Value>> &m) {
        return FindAndReplace(m, result);
    }

    virtual std::vector<PGVT> RValues() { return {}; }
};

struct Getelementptr : Instr {
    // <result> = getelementptr <ty>, <typtr> <ptrval>, <ty1> <idx1>, ...
    std::shared_ptr<Type> ty;
    std::shared_ptr<Value> ptr;
    std::vector<std::shared_ptr<Value>> indices;
    std::shared_ptr<Value> result;
    Getelementptr() : Instr(kOpGetelementptr) {}
    Getelementptr(std::shared_ptr<Type> ty, std::shared_ptr<Value> ptr,
                  std::vector<std::shared_ptr<Value>> indices,
                  std::shared_ptr<Value> result)
        : Instr(kOpGetelementptr), ty(ty), ptr(ptr), indices(indices),
          result(result) {}

    virtual std::string str() {
        assert(indices.size() > 0 && "getelementptr indices size can not be 0");
        std::string s = result->str() + " = getelementptr " + ty->str() + ", " +
                        ptr->typed_str() + ", ";
        foreach (
            indices, FOREACH_LAMBDA() { s += (*it)->typed_str(); },
            FOREACH_LAMBDA() { s += ", "; })
            ;
        return s;
    }

    virtual bool HasResult() const { return result.operator bool(); }
    virtual std::shared_ptr<Value> Result() { return result; }
    virtual int ReplaceValue(std::shared_ptr<Value> old,
                             std::shared_ptr<Value> _new) {
        auto p = old.get();
        int res = 0;
        if (ptr.get() == p)
            ptr = _new, res++;
        for (auto &idx : indices)
            if (idx.get() == p)
                idx = _new, res++;
        if (result.get() == p)
            result = _new, res++;
        return res;
    }

    virtual int
    ReplaceValues(std::map<std::shared_ptr<Value>, std::shared_ptr<Value>> &m) {
        int res = 0;
        res += FindAndReplace(m, ptr);
        for (auto &idx : indices)
            res += FindAndReplace(m, idx);
        res += FindAndReplace(m, result);
        return res;
    }

    virtual std::vector<PGVT> RValues() {
        std::vector<PGVT> rvals{(PGVT)&ptr};
        for (auto &idx : indices)
            rvals.push_back(&idx);
        return rvals;
    }
};

struct Phi : Instr {
    // <result> = phi <ty> [ <val1>, <label1>], ...

    struct PhiVal {
        std::shared_ptr<Value> val;
        std::shared_ptr<LocalValue> label;

        std::string str() {
            return "[" + val->str() + ", " + label->str() + "]";
        }
    };

    std::shared_ptr<Type> ty;
    std::shared_ptr<Value> result;
    std::vector<PhiVal> vals;
    Phi() : Instr(kOpPhi) {}

    virtual std::string str() {
        std::string s = result->str() + " = phi " + ty->str() + " ";
        foreach (
            vals, FOREACH_LAMBDA() { s += it->str(); },
            FOREACH_LAMBDA() { s += ", "; })
            ;
        return s;
    }

    virtual bool HasResult() const { return result.operator bool(); }
    virtual std::shared_ptr<Value> Result() { return result; }
    virtual int ReplaceValue(std::shared_ptr<Value> old,
                             std::shared_ptr<Value> _new) {
        auto p = old.get();
        int res = 0;
        if (result.get() == p)
            result = _new, res++;
        for (auto &pv : vals) {
            if (pv.label.get() == p)
                pv.label = std::dynamic_pointer_cast<LocalValue>(_new), res++;
            if (pv.val.get() == p)
                pv.val = _new, res++;
        }
        return res;
    }

    virtual int
    ReplaceValues(std::map<std::shared_ptr<Value>, std::shared_ptr<Value>> &m) {
        int res = 0;
        res += FindAndReplace(m, result);
        for (auto &pv : vals) {
            res += FindAndReplace(m, pv.label);
            res += FindAndReplace(m, pv.val);
        }
        return res;
    }

    virtual std::vector<PGVT> RValues() {
        std::vector<PGVT> rvals;
        for (auto &v : vals) {
            rvals.push_back((PGVT)&v.label);
            rvals.push_back(&v.val);
        }
        return rvals;
    }
};

struct Zext : Instr {
    // <result> = zext <ty> <val> to <ty_ext>
    std::shared_ptr<Type> ty_ext;
    std::shared_ptr<Value> val;
    std::shared_ptr<Value> result;
    Zext() : Instr(kOpZext) {}
    Zext(std::shared_ptr<Type> ty_ext, std::shared_ptr<Value> val,
         std::shared_ptr<Value> result)
        : Instr(kOpZext), ty_ext(ty_ext), val(val), result(result) {}

    virtual std::string str() {
        return result->str() + " = zext " + val->typed_str() + " to " +
               ty_ext->str();
    }

    virtual bool HasResult() const { return result.operator bool(); }
    virtual std::shared_ptr<Value> Result() { return result; }
    virtual int ReplaceValue(std::shared_ptr<Value> old,
                             std::shared_ptr<Value> _new) {
        auto p = old.get();
        int res = 0;
        if (val.get() == p)
            val = _new, res++;
        if (result.get() == p)
            result = _new, res++;
        return res;
    }

    virtual int
    ReplaceValues(std::map<std::shared_ptr<Value>, std::shared_ptr<Value>> &m) {
        int res = 0;
        res += FindAndReplace(m, val);
        res += FindAndReplace(m, result);
        return res;
    }

    virtual std::vector<PGVT> RValues() { return {&val}; }
};

struct Bitcast : Instr {
    // <result> = bitcast <ty> <val> to <ty_cast>
    std::shared_ptr<Type> ty_cast;
    std::shared_ptr<Value> val;
    std::shared_ptr<Value> result;
    Bitcast() : Instr(kOpBitcast) {}
    Bitcast(std::shared_ptr<Type> ty_cast, std::shared_ptr<Value> val,
            std::shared_ptr<Value> result)
        : Instr(kOpBitcast), ty_cast(ty_cast), val(val), result(result) {}

    virtual std::string str() {
        return result->str() + " = bitcast " + val->typed_str() + " to " +
               ty_cast->str();
    }

    virtual bool HasResult() const { return result.operator bool(); }
    virtual std::shared_ptr<Value> Result() { return result; }

    virtual int ReplaceValue(std::shared_ptr<Value> old,
                             std::shared_ptr<Value> _new) {
        auto p = old.get();
        int res = 0;
        if (val.get() == p)
            val = _new, res++;
        if (result.get() == p)
            result = _new, res++;
        return res;
    }

    virtual int
    ReplaceValues(std::map<std::shared_ptr<Value>, std::shared_ptr<Value>> &m) {
        int res = 0;
        res += FindAndReplace(m, val);
        res += FindAndReplace(m, result);
        return res;
    }

    virtual std::vector<PGVT> RValues() { return {&val}; }
};

struct FuncDecl {
    std::string name;
    std::shared_ptr<FuncT> ft;

    FuncDecl() {}
    FuncDecl(std::string name) : name(name) {}

    virtual std::string str() {
        std::string s = "declare " + ft->ret->str() + " @" + name + "(";
        foreach (
            ft->args, FOREACH_LAMBDA() { s += (*it)->str(); },
            FOREACH_LAMBDA() { s += ", "; })
            ;
        s += ")";
        return s;
    }
};

struct Define {
    bool is_const;
    std::shared_ptr<GlobalVar> var;
    std::unique_ptr<InitVal> init;

    Define() { is_const = false; }

    virtual std::string str();
};

// Basic Block
struct BB {
    int tag;
    enum { kDefault, kLoopHeader, kLoopBody, kLoopExit, kRet };
    // instructions
    std::vector<std::unique_ptr<Instr>> insts;
    std::set<BB *> successors;


    std::vector<BB *> predecessors;
    std::shared_ptr<LocalValue> label;
    Func *func;
    int id;
    //增加判断基本块中的指令是否为空
    bool empty() { return insts.empty(); }

    struct BBExtraInfo {
        template <typename T> T *cast() { return static_cast<T *>(this); }
    };
    std::vector<std::unique_ptr<BBExtraInfo>> extra;

    template <typename T, typename... ARGS> T *CreateInstr(ARGS... args) {
        auto inst = std::make_unique<T>(args...);
        auto p = inst.get();
        insts.push_back(std::move(inst));
        return p;
    }




            
    

    virtual void dump(std::ostream &os);

    virtual ~BB() = default;
};

struct Func {
  public:
    int tmp_var_id;

    std::shared_ptr<FuncT> ft;
    std::string name;
    // function arguments, refered as local varible
    std::vector<std::shared_ptr<LocalValue>> args;
    // basic blocks
    std::vector<std::unique_ptr<BB>> bblocks;

    //张启涵：额外添加map数据结构
    std::map<BB *, std::set<BB *>> _successorMap;


    // variables
    // used to find var
    std::map<std::string, std::shared_ptr<LocalVar>> vars;
    std::map<Value *, BB *> label_map;
    Module *m;

    BB *CreateBB() {
        auto bb = std::make_unique<BB>();
        auto pbb = bb.get();
        bblocks.push_back(std::move(bb));
        return pbb;
    }

    std::shared_ptr<LocalVar> CreateLocalVar(std::shared_ptr<Type> ty,
                                             std::string name) {
        auto it = vars.find(name);
        if (it != vars.end()) {
            assert(it->second->ty->eq(*ty) && "local var re-define");
            return it->second;
        }
        auto var = std::make_shared<LocalVar>(ty, name);
        vars.insert({name, var});
        return var;
    }

    std::shared_ptr<TmpVar> CreateTmpVar(std::shared_ptr<Type> ty) {
        return std::make_shared<TmpVar>(ty);
    }

    // reset basic block id
    void ResetBBID() {
        int id = 0;
        for (auto &bb : bblocks)
            bb->id = id++;
    }
    //张启涵：增加getDomTreeSuccessorBlocks函数
    std::set<BB *> getDomTreeSuccessorBlocks(BB *bb) { return _successorMap[bb]; }

    //张启涵：增加isDominatedBy函数，用于检查前者是否被后者支配
        bool isDominatedBy(BB *child, BB *parent) {
        if (child == parent) return true;
        //还需要额外增加函数
        auto sets = getDomTreeSuccessorBlocks(parent);
        return sets.find(child) != sets.end();
    }

    // call it whenever you want to refresh tmp var id
    void ResetTmpVar() {
        tmp_var_id = 0;
        for (auto x : args)
            CheckNewTmpVarDefine(x);

        bool first_bb = true;
        for (auto &bb : bblocks) {
            if (bb->label) {
                CheckNewTmpVarDefine(bb->label);
            } else if (first_bb) {
                tmp_var_id++;
            }
            for (auto &inst : bb->insts) {
                if (inst->HasResult())
                    CheckNewTmpVarDefine(inst->Result());
            }
            first_bb = false;
        }
    }

    void CheckNewTmpVarDefine(std::shared_ptr<Value> v) {
        if (v->kind == Value::kTmpVar) {
            std::dynamic_pointer_cast<TmpVar>(v)->id = tmp_var_id++;
        }
    }

    std::shared_ptr<Value> FindVar(std::string var, bool only_in_func = false);

    //增加判断是否为declaration
    bool is_declaration() { return bblocks.empty(); }

    virtual void dump(std::ostream &os);
};

struct Module {

    //张启涵：由于增加活跃变量算法，增加如下的数据结构

    std::map<BB*,std::unordered_set<Value*> > active, live_in, live_out;
    std::map<BB*,std::unordered_set<Value*> > use, def, phi_op;
    std::map<BB*,std::set<std::pair<BB *,Value  *>>> phi_op_pair;


    struct {
        std::map<std::string, FuncDecl *> func_decls;
        std::map<std::string, Func *> funcs;
    } query;
    std::vector<std::unique_ptr<FuncDecl>> func_decls;
    std::vector<std::unique_ptr<Func>> funcs;
    std::vector<std::unique_ptr<Define>> defs;
    std::map<std::string, std::shared_ptr<GlobalVar>> vars;
    std::map<int, std::shared_ptr<ImmValue>> imms;

    std::shared_ptr<GlobalVar> CreateGlobalVar(std::shared_ptr<Type> ty,
                                               std::string name) {
        auto it = vars.find(name);
        if (it != vars.end())
            return it->second;
        auto var = std::make_shared<GlobalVar>(ty, name);
        vars.insert({name, var});
        return var;
    }

    std::shared_ptr<ImmValue> CreateImmI32(int v) {
        auto it = imms.find(v);
        if (it != imms.end())
            return it->second;
        auto imm = std::make_shared<ImmValue>(t_i32, v);
        imms.insert({v, imm});
        return imm;
    }

    std::shared_ptr<ImmValue> CreateImm(int v,
                                        std::shared_ptr<ir::Type> ty = t_i32) {
        if (ty->kind == Type::kI1) {
            return v == 0 ? ImmValue::i1_zero : ImmValue::i1_one;
        } else if (ty->kind == Type::kI32) {
            return CreateImmI32(v);
        } else {
            return std::make_shared<ImmValue>(ty, v);
        }
    }

    std::shared_ptr<Value> FindVar(std::string var) { return vars[var]; }

    std::shared_ptr<FuncT> FindFuncT(std::string name) {
        auto p = query.funcs.find(name);
        if (p != query.funcs.end())
            return p->second->ft;
        auto ppdef = query.func_decls.find(name);
        if (ppdef != query.func_decls.end())
            return ppdef->second->ft;
        return nullptr;
    }

    auto InsertFuncDecl(FuncDecl *func_decl) {
        auto r = query.func_decls.insert({func_decl->name, func_decl});
        if (r.second) {
            func_decls.push_back(std::unique_ptr<FuncDecl>(func_decl));
            vars[func_decl->name] =
                CreateGlobalVar(func_decl->ft, func_decl->name);
        }
        return r;
    }

    void DeleteFuncDecl(FuncDecl *func_decl) {
        auto it = query.func_decls.find(func_decl->name);
        if (it != query.func_decls.end()) {
            vars.erase(func_decl->name);
            FindAndDelete(func_decls, func_decl);
            query.func_decls.erase(it);
        }
    }

    auto InsertFunc(Func *f) {
        auto r = query.funcs.insert({f->name, f});
        if (r.second) {
            funcs.push_back(std::unique_ptr<Func>(f));
            vars[f->name] = CreateGlobalVar(f->ft, f->name);
        }
        return r;
    }

    void DeleteFuncDecl(Func *f) {
        auto it = query.funcs.find(f->name);
        if (it != query.funcs.end()) {
            vars.erase(f->name);
            FindAndDelete(funcs, f);
            query.funcs.erase(it);
        }
    }

    virtual void dump(std::ostream &os);

  private:
    template <typename T>
    void FindAndDelete(std::vector<std::unique_ptr<T>> &v, T *p) {
        for (auto it = v.begin(); it != v.end(); it++) {
            if ((*it).get() == p) {
                v.erase(it);
                break;
            }
        }
    }

    void test() {}
};

} // namespace ir

#endif
