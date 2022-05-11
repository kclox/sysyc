#ifndef __ast_ast_hpp__
#define __ast_ast_hpp__

#include "ast/type.h"
#include "type_utils.h"
#include "utils.h"
#include "json.hpp"
#include <assert.h>
#include <functional>
#include <memory>
#include <string.h>
#include <string>

namespace ast {

/* Declarations */
struct AstNode;
struct Decl;
struct Func;
struct Stmt;
struct AssignStmt;
struct DeclStmt;
struct WhileStmt;
struct BreakStmt;
struct ContinueStmt;
struct IfStmt;
struct BlockStmt;
struct ExpStmt;
struct ReturnStmt;
struct Exp;
struct ConstExp;
struct VarExp;
struct CallExp;
struct UnaryExp;
struct BinaryExp;
struct ArrayInitVal;
struct DefWithInit;
struct Array;
struct ArrayInitItem;
struct ArrayInitRecord;

using json_t = nlohmann::json;

const char *Type2Str(int type);

struct Visitor {
    bool visit_child_before;

    Visitor() { visit_child_before = false; }

#define VisitChildBeforeRoutine()                                              \
    {                                                                          \
        if (visit_child_before) {                                              \
            visit_child_before = false;                                        \
            return true;                                                       \
        }                                                                      \
    }

    // Visit AstNode, return true if you want to continue visiting child nodes
    virtual bool Visit(AstNode *node) = 0;

    void VisitChildBefore(AstNode *node);
};

struct AstNode {
  public:
    int op;
    enum {
        kOpBDecl,
        kOpArrayDecl,
        kOpFuncDecl,

        kOpFunc,

        kOpNop,
        kOpAssign,
        kOpDeclStmt,
        kOpWhile,
        kOpBreak,
        kOpContinue,
        kOpIf,
        kOpBlock,
        kOpExpStmt,
        kOpReturn,

        kOpConst,
        kOpVar,
        kOpCall,
        kOpRef,
        kOpAdd,
        kOpSub,
        kOpMul,
        kOpDiv,
        kOpMod,
        kOpGt,
        kOpLt,
        kOpEq,
        kOpNe,
        kOpGe,
        kOpLe,
        kOpLAnd,
        kOpLOr,
        kOpNot,
        kOpUMinus,
        kOpUAdd,

        kOpArrayInit,
    };

    static const std::string Op2Str(int op);
    const std::string OpStr() const { return Op2Str(op); }
    bool IsDecl() const { return op >= kOpBDecl && op <= kOpFuncDecl; }
    bool IsExp() const { return op >= kOpConst && op <= kOpUAdd; }
    bool IsBinaryExp() const { return op >= kOpAdd && op <= kOpLOr; }
    bool IsUnaryExp() const { return op >= kOpNot && op <= kOpUAdd; }
    bool IsCondExp() const { return op >= kOpGt && op <= kOpNot; }
    bool IsCompExp() const { return op >= kOpGt && op <= kOpLe; }
    bool IsStmt() const { return op >= kOpNop && op <= kOpReturn; }

    AstNode() {}
    AstNode(int op) : op(op) {}

    virtual operator json_t() const { return to_json(); }
    virtual json_t to_json() const { return {{"op", OpStr()}}; }

    virtual void Accept(Visitor *visitor) { visitor->Visit(this); }

    template <typename T> T *dyn() { return static_cast<T *>(this); }
};

/* Declaration */
struct Decl : public AstNode {

    bool is_const;
    Type *type;
    std::string name;

    Decl(Type *type, std::string name, bool is_const = false)
        : type(type), name(name), is_const(false) {}

    virtual bool IsFunc() { return false; }
    virtual bool IsConst() { return is_const; }
    virtual bool HasInitVal() const = 0;
    virtual void SetAttr(Type *type = nullptr, bool is_const = false) {
        this->type = type;
        this->is_const = is_const;
    }

    virtual json_t to_json() const {
        auto _json = AstNode::to_json();
        _json.merge_patch({{"is_const", is_const},
                           {"type", type->to_json()},
                           {"name", name}});
        return _json;
    }
};

struct BDecl : public Decl {
    bool global_init_evaluated;
    Exp *init;

  public:
    BDecl(Type *type, std::string name, Exp *init = nullptr)
        : Decl(type, name) {
        this->op = kOpBDecl;
        this->init = init;
        global_init_evaluated = false;
    }

    bool HasInitVal() const { return init != nullptr; }

    Exp *InitVal();

    virtual json_t to_json() const;
    void EvalGlobalInit(Scope<Decl *> *scope);
    virtual void Accept(Visitor *visitor);
};

struct ArrayDecl : public Decl {
    // array type maturity
    // 0: init, this->ty is element type
    // 1: this->ty is array type, but count is incomplete
    // 2: this->ty is array type, and count is set
    int array_type_maturity;
    Array *array;
    bool global_init_evaluated;
    ArrayInitVal *init;
    bool array_init_record_set;
    std::vector<ArrayInitRecord> init_val_records;

  public:
    ArrayDecl(Type *type, std::string name, Array *array,
              ArrayInitVal *init = nullptr)
        : Decl(type, name), array(array) {
        this->op = kOpArrayDecl;
        this->init = init;
        array_type_maturity = 0;
        global_init_evaluated = false;
        array_init_record_set = false;
    }

    // can only be invoked after inited
    ArrayT *GetType() { return static_cast<ArrayT *>(type); }

    // can only be invoked after inited
    Exp *GetInitVal(int idx);

    virtual bool HasInitVal() const { return init != nullptr; }

    virtual json_t to_json() const;

    // after that, array_type_maturity = 1
    void InitArrayType(Scope<Decl *> *scope, bool is_func_arg = false);
    void EvalGlobalInit(Scope<Decl *> *scope);
    // after that, array_type_maturity = 2
    void InitArrayInitRecords(bool is_func_arg = false);

    virtual void Accept(Visitor *visitor);
};

struct FuncDecl : public Decl {
    ast::FuncT ft;

    FuncDecl(std::string name, Type *ret, std::initializer_list<Type *> args)
        : Decl(&ft, name) {
        // &ft => may bug
        this->op = kOpFuncDecl;
        this->ft.ret = ret;
        for (auto x : args)
            this->ft.args.push_back(x);
    }

    FuncDecl(std::string name, Type *ret, std::vector<Type *> args)
        : Decl(&ft, name) {
        // &ft => may bug
        this->op = kOpFuncDecl;
        this->ft.ret = ret;
        this->ft.args = args;
    }

    virtual bool IsFunc() { return true; }

    virtual bool HasInitVal() const { return false; }
};

/* Statement */
struct Stmt : public AstNode {
    // statement belongs to block
    // but, there are some block statements
    // which do not belong to any block
    // like block of func code
    BlockStmt *parent;

    Stmt(BlockStmt *parent = nullptr) : parent(parent) {}
    virtual ~Stmt() {}

    virtual void SetParent(BlockStmt *parent) {
        if (parent)
            assert(false && "parent can only be set once");
        this->parent = parent;
    }
};

struct NopStmt : public Stmt {
    NopStmt() { op = kOpNop; }
};

struct AssignStmt : public Stmt {
    Exp *lval, *rval;

    AssignStmt(Exp *lval, Exp *rval) : lval(lval), rval(rval) {
        this->op = kOpAssign;
    }

    virtual void Accept(Visitor *visitor);

    virtual json_t to_json() const;
};

struct DeclStmt : public Stmt {
    List<Decl *> decls;

    DeclStmt(List<Decl *> decls) : decls(decls) { op = kOpDeclStmt; }

    virtual void Accept(Visitor *visitor);

    virtual json_t to_json() const;
};

struct WhileStmt : public Stmt {
    Exp *cond;
    Stmt *stmt;

    WhileStmt(Exp *cond, Stmt *stmt) : cond(cond), stmt(stmt) { op = kOpWhile; }

    virtual json_t to_json() const;

    virtual void Accept(Visitor *visitor);
};

struct BreakStmt : public Stmt {
    BreakStmt() { op = kOpBreak; }
};

struct ContinueStmt : public Stmt {
    ContinueStmt() { op = kOpContinue; }
};

struct IfStmt : public Stmt {
    Exp *cond;
    Stmt *tstmt, *fstmt;

    IfStmt(Exp *cond, Stmt *t, Stmt *f = nullptr)
        : cond(cond), tstmt(t), fstmt(f) {
        op = kOpIf;
    }

    virtual json_t to_json() const;

    virtual void Accept(Visitor *visitor);
};

struct BlockStmt : public Stmt {
    List<Stmt *> stmts;
    Func *func;
    int id;

    BlockStmt(List<Stmt *> stmts, Func *func = nullptr) : stmts(stmts) {
        this->op = kOpBlock;
        this->func = func;
        SetBlockID();
    }

    //
    // virtual void SetParent(BlockStmt *parent) {
    //     Stmt::SetParent(parent);
    //     for (auto x : stmts)
    //         x->SetParent(parent);
    // }

    void SetFunc(Func *func) {
        if (this->func)
            assert(false && "func can only be set once");
        this->func = func;
        SetBlockID();
    }

    virtual json_t to_json() const;

    virtual void Accept(Visitor *visitor);

  private:
    void SetBlockID();
};

struct ExpStmt : public Stmt {
    Exp *exp;

    ExpStmt(Exp *exp) : exp(exp) { op = kOpExpStmt; }

    virtual json_t to_json() const;

    virtual void Accept(Visitor *visitor);
};

struct ReturnStmt : public Stmt {
    Exp *val;

    ReturnStmt(Exp *val = nullptr) : val(val) { op = kOpReturn; }

    virtual json_t to_json() const;

    virtual void Accept(Visitor *visitor);
};

/* Function */
struct Func : public AstNode {
    FuncT ft;
    std::string name;
    List<Decl *> args;
    BlockStmt *block;
    int block_count;

    Func(std::string name, Type *ret, List<Decl *> args, BlockStmt *block)
        : name(name), args(args), block(block), block_count(0) {
        this->op = kOpFunc;
        ft.ret = ret;
        for (auto x : args) {
            ft.args.push_back(x->type);
        }
    }

    int AddBlock() { return ++block_count; }

    virtual json_t to_json() const;

    virtual void Accept(Visitor *visitor);
};

/* Expression */
struct ArrayInitItem : public AstNode {
    int kind;
    enum { kExp, kList };

    ArrayInitItem(int kind) : kind(kind) {}
};

struct Exp : public ArrayInitItem {
    struct EvalParams {
        Scope<Decl *> *scope;
        bool must_const;
    };

  public:
    Exp() : ArrayInitItem(kExp) {}

    virtual int eval(EvalParams params) { return 0; };
};

struct ConstExp : public Exp {
    int val;
    ConstExp() { this->op = kOpConst; }
    ConstExp(int val) : ConstExp() { this->val = val; }
    virtual int eval(EvalParams params) { return val; }

    virtual json_t to_json() const;

    static ConstExp *zero;
};

struct VarExp : public Exp {
    std::string name;

    VarExp(std::string name) : name(name) { op = kOpVar; }

    virtual int eval(EvalParams params);

    virtual json_t to_json() const;
};

struct CallExp : public Exp {
    std::string name;
    List<Exp *> args;

    CallExp(std::string name, List<Exp *> args) : name(name), args(args) {
        op = kOpCall;
    }

    virtual json_t to_json() const;

    virtual void Accept(Visitor *visitor);
};

struct RefExp : public Exp {
    std::string name;
    Array *ref;

    RefExp(std::string name, Array *ref) : name(name), ref(ref) {
        this->op = kOpRef;
    }

    virtual json_t to_json() const;

    virtual int eval(EvalParams params);

    virtual void Accept(Visitor *visitor);
};

struct UnaryExp : public Exp {
    Exp *arg;

    UnaryExp(int op, Exp *arg) : arg(arg) { this->op = op; }

    virtual json_t to_json() const;

    virtual int eval(EvalParams params);

    virtual void Accept(Visitor *visitor);
};

struct BinaryExp : public Exp {
    Exp *left, *right;

    BinaryExp(int op, Exp *left, Exp *right) : left(left), right(right) {
        this->op = op;
    }

    virtual json_t to_json() const;

    virtual int eval(EvalParams params);

    virtual void Accept(Visitor *visitor);
};

/* array */
struct Array {
    std::vector<Exp *> dimensions;

    Array(Exp *e) { this->dimensions.push_back(e); }

    virtual json_t to_json() const;

    void VisitDimExps(Visitor *visitor);
};

struct ArrayInitVal : public ArrayInitItem {
    List<ArrayInitItem *> children;

    ArrayInitVal() : ArrayInitItem(kList) { this->op = kOpArrayInit; }

    ArrayInitVal(List<ArrayInitItem *> children)
        : children(children), ArrayInitItem(kList) {
        this->op = kOpArrayInit;
    }

    virtual json_t to_json() const;

    virtual void Accept(Visitor *visitor);
};

struct ArrayInitRecord {
    int pos;
    std::vector<Exp *> vals;

    virtual json_t to_json() {
        json_t j;
        j["pos"] = pos;
        j["vals"] = {};
        for (auto x : vals)
            j["vals"].push_back(x->to_json());
        return j;
    }

    virtual operator json_t() { return to_json(); }

    void Reset() {
        pos = 0;
        vals.clear();
    }
};

struct CompUnit {
    mapped_vector<std::string, Decl *> decls;
    mapped_vector<std::string, Func *> funcs;

    virtual json_t to_json() const {
        json_t j;
        j["decls"] = {};
        j["funcs"] = {};
        for (auto x : decls)
            j["decls"].push_back(x->to_json());
        for (auto x : funcs)
            j["funcs"].push_back(x->to_json());
        return j;
    }
};

} // namespace ast

#endif
