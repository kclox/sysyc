/*
 * Semantic Check
 *  1. check break/continue in while statement
 *  2. check use before define
 *  3. calculate init value
 */

#include "ast/ast.hpp"
#include "builtin.hpp"
#include "error.h"

using namespace ast;

struct SemanticCheckVisitor : Visitor {
  private:
    CompUnit *comp_unit;
    Scope<Decl *> *current_scope;
    bool in_while;
    Func *func;
    BlockStmt *block;
    bool is_func_arg;
    bool is_global;

    inline void EnterScope() {
        current_scope = new Scope<Decl *>(current_scope);
    }

    inline void LeaveScope() {
        auto sc = current_scope->parent;
        delete current_scope;
        current_scope = sc;
    }

  public:
    SemanticCheckVisitor(CompUnit *c) {
        comp_unit = c;
        in_while = false;
        current_scope = new Scope<Decl *>();
        func = nullptr;
        block = nullptr;
        is_func_arg = false;
    }

    ~SemanticCheckVisitor() { delete current_scope; }

    void Check() {
        VisitGlobalDecls();
        VisitFuncs();
    }

    virtual bool Visit(AstNode *node) {
        VisitChildBeforeRoutine();
        if (node->IsStmt())
            node->dyn<Stmt>()->SetParent(block);
        switch (node->op) {
        /// for function
        case AstNode::kOpFunc: {
            func = node->dyn<Func>();
            // create scope for args
            EnterScope();
            is_func_arg = true;
            int i = 0;
            for (auto x : func->args) {
                x->Accept(this);
                // set array type
                func->ft.args[i++] = x->type;
            }
            is_func_arg = false;
            func->block->Accept(this);
            LeaveScope();
            func = nullptr;
            return false;
        }
        /// for declare
        case AstNode::kOpBDecl: {
            auto decl = node->dyn<BDecl>();
            // visit child to check use-before-define
            VisitChildBefore(node);
            // set init value
            if (decl->HasInitVal() && is_global) {
                decl->EvalGlobalInit(current_scope);
            }
            current_scope->Insert(decl);
            return false;
        }
        case AstNode::kOpArrayDecl: {
            auto decl = node->dyn<ArrayDecl>();
            VisitChildBefore(node);
            if (is_global) {
                if (decl->HasInitVal()) {
                    decl->EvalGlobalInit(current_scope);
                }
            }
            decl->InitArrayType(current_scope, is_func_arg);
            decl->InitArrayInitRecords(is_func_arg);
            current_scope->Insert(decl);
            return false;
        }
        case AstNode::kOpFuncDecl: {
            current_scope->Insert(node->dyn<FuncDecl>());
            return false;
        }
        /// for block
        case AstNode::kOpBlock: {
            EnterScope();
            node->dyn<BlockStmt>()->SetFunc(func);
            VisitChildBefore(node);
            LeaveScope();
            return false;
        }
        /// for var and array
        case AstNode::kOpVar: {
            auto var = node->dyn<VarExp>();
            if (!current_scope->Find(var->name)) {
                throw SymbolNotDefine(var->name);
            }
            return false;
        }
        case AstNode::kOpCall: {
            auto call = node->dyn<CallExp>();
            auto ppf = comp_unit->funcs.find(call->name);
            if (!ppf) {
                auto ppdecl = comp_unit->decls.find(call->name);
                if (!ppdecl) {
                    auto it = ast_builtin_funcs.find(call->name);
                    if (it != ast_builtin_funcs.end()) {
                        comp_unit->decls.insert(call->name, it->second);
                    } else {
                        throw SymbolNotDefine(call->name);
                    }
                }
            }
        } break;
        case AstNode::kOpRef: {
            auto arr = node->dyn<RefExp>();
            if (!current_scope->Find(arr->name)) {
                throw SymbolNotDefine(arr->name);
            }
        } break; // visit children
        /// for while
        case AstNode::kOpWhile: {
            bool old_in_while = in_while;
            in_while = true;
            VisitChildBefore(node);
            in_while = old_in_while;
            return false;
        }
        /// for break/continue
        case AstNode::kOpBreak:
        case AstNode::kOpContinue: {
            if (!in_while) {
                throw BreakContiException(
                    node->op == AstNode::kOpBreak ? "break" : "continue");
            }
            return false;
        }
        }
        return true;
    }

  private:
    void VisitGlobalDecls() {
        is_global = true;
        assert(current_scope->size() == 0);
        for (auto decl : comp_unit->decls) {
            decl->Accept(this);
        }
        is_global = false;
    }

    void VisitFuncs() {
        for (auto func : comp_unit->funcs) {
            func->Accept(this);
        }
    }
};

CompUnit *SemanticCheck(CompUnit *comp_unit) {
    SemanticCheckVisitor sc(comp_unit);
    sc.Check();
    return comp_unit;
}
