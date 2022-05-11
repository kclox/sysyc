#include "ast/ast.hpp"
#include "ir/ir.hpp"
#include "utils.h"
#include "builtin.hpp"
#include <stack>

static int AstOp2IrIcmpCond(int op) {
    const int m[] = {
        ir::Icmp::kSgt, ir::Icmp::kSlt, ir::Icmp::kEq,
        ir::Icmp::kNe,  ir::Icmp::kSge, ir::Icmp::kSle,
    };
    assert(op >= ast::AstNode::kOpGt && op <= ast::AstNode::kOpLe);
    return m[op - ast::AstNode::kOpGt];
}

static int AstAluOp2IrOp(int op) {
    const std::map<int, int> m = {
        {ast::AstNode::kOpAdd, ir::Instr::kOpAdd},
        {ast::AstNode::kOpSub, ir::Instr::kOpSub},
        {ast::AstNode::kOpMul, ir::Instr::kOpMul},
        {ast::AstNode::kOpDiv, ir::Instr::kOpSdiv},
        {ast::AstNode::kOpMod, ir::Instr::kOpSrem},
    };
    auto it = m.find(op);
    return it->second;
}

static std::shared_ptr<ir::Type> AstType2IrType(ast::Type *ast_ty,
                                                bool is_func_arg = false) {
    switch (ast_ty->kind) {
    case ast::Type::kInt: {
        return ir::t_i32;
    }
    case ast::Type::kVoid: {
        return ir::t_void;
    }
    case ast::Type::kNudef: {
        return ir::t_undef;
    }
    case ast::Type::kArray: {
        auto arr = dynamic_cast<ast::ArrayT *>(ast_ty);
        auto p = AstType2IrType(arr->element);
        for (auto it = arr->dimensions.rbegin(); it != arr->dimensions.rend();
             it++) {
            int count = *it;
            if (it + 1 == arr->dimensions.rend()) {
                // first dimension
                if (is_func_arg || count == 0) {
                    p = std::make_shared<ir::PtrT>(p);
                    break;
                }
            }
            assert(count != 0);
            p = std::make_shared<ir::ArrayT>(p, count);
        }
        return p;
    }
    case ast::Type::kFunc: {
        auto ast_ft = dynamic_cast<ast::FuncT *>(ast_ty);
        auto ir_ft = std::make_shared<ir::FuncT>();
        ir_ft->ret = AstType2IrType(ast_ft->ret);
        for (auto x : ast_ft->args) {
            ir_ft->args.push_back(AstType2IrType(x, true));
        }
        return ir_ft;
    }
    default:
        assert(false);
    }
}

std::shared_ptr<ir::ArrayT> AstArrayTy2OneDimIrArrayTy(ast::ArrayT *ast_ty) {
    return std::make_shared<ir::ArrayT>(AstType2IrType(ast_ty->element),
                                        ast_ty->Count());
}

struct IrBuildVisitor : ast::Visitor {
    ast::CompUnit *comp_unit;
    Scope<ast::Decl *> *current_scope;
    Scope<ast::Decl *> *global;

    // last instruction result
    std::shared_ptr<ir::Value> res;

    struct Ctx {
        bool active;
    };
    template <typename T> struct CtxStack {
        std::stack<T> s;

        CtxStack() { s.push(T()); }

        inline void Do(std::function<void(T &)> f) {
            s.push(s.top());
            s.top().active = true;
            f(s.top());
            s.pop();
        }

        void push() { s.push(Current()); }

        void pop() { s.pop(); }

        inline T &Current() { return s.top(); }

        inline bool IsActive() { return Current().active; }
    };

    struct CondCtx : public Ctx {
        std::shared_ptr<ir::LocalValue> tlabel, flabel;
    };
    CtxStack<CondCtx> cond_stack;

    struct WhileCtx : public Ctx {
        std::shared_ptr<ir::LocalValue> header, body, exit;
    };
    CtxStack<WhileCtx> while_stack;

    struct VarCtx : public Ctx {
        bool need_load;
        VarCtx() {
            active = true;
            need_load = true;
        }
    };
    CtxStack<VarCtx> varctx_stack;

    ast::Func *ast_func;
    ir::Func *ir_func;
    ir::BB *bb;
    ir::Module &m;

    std::map<ir::Value *, std::shared_ptr<ir::PtrT>> ast_array_type_map;
    std::map<ast::Decl *, std::shared_ptr<ir::Value>> ast_ir_decl_map;
    std::map<std::string, std::string> func_name_map;

    bool has_add_set_i32_array;

    struct {
        std::string prefix;
        std::stack<int> prev_size;

        void PushPrefix(std::string x) {
            prev_size.push(prefix.length());
            if (prefix.length() > 0)
                prefix += "." + x;
            else
                prefix = x;
        }

        void PopPrefix() {
            int prev = prev_size.top();
            prev_size.pop();
            prefix = prefix.substr(0, prev);
        }

        std::string Var(std::string name) {
            auto s = prefix + "." + name;
            if (s.length() >= 1024)
                return Hash(s);
            else
                return s;
        }

      private:
        std::string Hash(std::string s) {
            char buf[0x20] = {0};
            snprintf(buf, 0x20 - 1, ".hash.%lx", std::hash<std::string>()(s));
            return std::string(buf);
        }
    } var_prefix;

    IrBuildVisitor(ir::Module &m, ast::CompUnit *comp_unit)
        : m(m), comp_unit(comp_unit) {
        res = nullptr;
        ast_func = nullptr;
        ir_func = nullptr;
        bb = nullptr;
        global = new Scope<ast::Decl *>();
        current_scope = global;
        has_add_set_i32_array = false;
    }

    ~IrBuildVisitor() { delete global; }

    void SetI32ArrayZero(std::shared_ptr<ir::Value> arr, int count) {
        if (!has_add_set_i32_array) {
            AddFuncDecl(nullptr, "set_i32_array");
            has_add_set_i32_array = true;
        }
        auto call = std::make_unique<ir::Call>();
        call->func = std::dynamic_pointer_cast<ir::GlobalVar>(
            m.FindVar(AstFuncNameToIr("set_i32_array")));
        call->args.push_back(m.CreateImm(0));
        call->args.push_back(AutoTypeCast(ir::t_pi32, arr));
        call->args.push_back(m.CreateImm(count));
        bb->insts.push_back(std::move(call));
    }

    inline void EnterScope(int id = 0, std::string name = "") {
        current_scope = new Scope<ast::Decl *>(current_scope);
        if (name.length() > 0)
            var_prefix.PushPrefix(name);
        else
            var_prefix.PushPrefix(std::to_string(id));
    }

    inline void LeaveScope() {
        auto sc = current_scope->parent;
        delete current_scope;
        current_scope = sc;
        var_prefix.PopPrefix();
    }

    void Build() {
        HandleDecls();
        HandleFuncs();
    }

    bool VisitBinaryExp(ast::BinaryExp *exp) {
        if (exp->IsCompExp()) {
            std::shared_ptr<ir::Value> l, r;
            cond_stack.Do([&](CondCtx &ctx) {
                ctx.active = false;
                exp->left->Accept(this);
                l = res;
                exp->right->Accept(this);
                r = res;
            });

            auto ty = AluSelectType(l->ty, r->ty);
            res = std::make_shared<ir::TmpVar>(ir::t_i1);
            bb->insts.push_back(std::make_unique<ir::Icmp>(
                AstOp2IrIcmpCond(exp->op), ty, AutoTypeCast(ty, l),
                AutoTypeCast(ty, r), res));
            LinkCondIfNeed();
        } else if (exp->op == ast::AstNode::kOpLAnd ||
                   exp->op == ast::AstNode::kOpLOr) {
            bool old_cond_ctx_active = cond_stack.IsActive();
            std::shared_ptr<ir::TmpVar> ptr;
            if (!old_cond_ctx_active) {
                cond_stack.push();
                auto &cur = cond_stack.Current();
                cur.active = true;
                cur.tlabel = std::make_shared<ir::TmpVar>(ir::t_label);
                cur.flabel = std::make_shared<ir::TmpVar>(ir::t_label);
                // ptr = std::make_shared<ir::TmpVar>(ir::t_pi32);
                // bb->insts.push_back(
                //     std::make_unique<ir::Alloca>(ir::t_i32, ptr, 4));
                // res = ptr;
                res = Alloca<ir::Value>(ir::t_i32);
            }
            if (exp->op == ast::AstNode::kOpLAnd) {
                // a && b
                //    a
                //   / \
                //   T0 F
                //  /
                //  b
                // / \
                // T F
                auto blabel = std::make_shared<ir::TmpVar>(ir::t_label);
                cond_stack.Do([&](CondCtx &ctx) {
                    ctx.tlabel = blabel;
                    exp->left->Accept(this);
                });
                bb = ir_func->CreateBB();
                bb->label = blabel;
                ir_func->label_map[blabel.get()] = bb;
                exp->right->Accept(this);
            } else {
                // a || b
                //    a
                //   / \
                //   T F0
                //      \
                //      b
                //     / \
                //    T   F
                auto blabel = std::make_shared<ir::TmpVar>(ir::t_label);
                cond_stack.Do([&](CondCtx &ctx) {
                    ctx.flabel = blabel;
                    exp->left->Accept(this);
                });
                bb = ir_func->CreateBB();
                bb->label = blabel;
                ir_func->label_map[blabel.get()] = bb;
                exp->right->Accept(this);
            }
            if (!old_cond_ctx_active) {
                auto next = std::make_shared<ir::TmpVar>(ir::t_label);
                auto &cur = cond_stack.Current();
                // T
                bb = ir_func->CreateBB();
                bb->label = cur.tlabel;
                ir_func->label_map[cur.tlabel.get()] = bb;
                bb->insts.push_back(std::make_unique<ir::Store>(res, ptr));
                bb->insts.push_back(std::make_unique<ir::Br>(next));

                // F
                bb = ir_func->CreateBB();
                bb->label = cur.flabel;
                ir_func->label_map[cur.flabel.get()] = bb;
                bb->insts.push_back(std::make_unique<ir::Store>(res, ptr));
                bb->insts.push_back(std::make_unique<ir::Br>(next));

                // next
                bb = ir_func->CreateBB();
                bb->label = next;
                ir_func->label_map[next.get()] = bb;
                res = std::make_shared<ir::TmpVar>(ir::t_i32);
                bb->insts.push_back(
                    std::make_unique<ir::Load>(ir::t_i32, ptr, res));
                cond_stack.pop();
            }
        } else {
            // other binary exp
            cond_stack.Do([&](CondCtx &ctx) {
                ctx.active = false;
                exp->left->Accept(this);
                auto l = res;
                exp->right->Accept(this);
                auto r = res;
                auto ty = AluSelectType(l->ty, r->ty);
                res = std::make_shared<ir::TmpVar>(ty);
                bb->insts.push_back(std::make_unique<ir::BinaryAlu>(
                    AstAluOp2IrOp(exp->op), ty, AutoTypeCast(ty, l),
                    AutoTypeCast(ty, r), res));
            });
            LinkCondIfNeed();
        }
        return false;
    }

    bool VisitUnaryExp(ast::UnaryExp *exp) {
        bool visit_child = false;
        if (cond_stack.IsActive()) {
            switch (exp->op) {
            case ast::AstNode::kOpNot: {
                cond_stack.Do([&](CondCtx &ctx) {
                    std::swap(ctx.tlabel, ctx.flabel);
                    exp->arg->Accept(this);
                });
            } break;
            case ast::AstNode::kOpUMinus:
            case ast::AstNode::kOpUAdd:
                visit_child = true;
                break;
            }
        } else {
            switch (exp->op) {
            case ast::AstNode::kOpNot: {
                exp->arg->Accept(this);
                auto val = res;
                res = std::make_shared<ir::TmpVar>(ir::t_i1);
                bb->insts.push_back(std::make_unique<ir::Icmp>(
                    ir::Icmp::kEq, AluSelectType(val->ty, ir::t_i32), val,
                    m.CreateImm(0), res));
            } break;
            case ast::AstNode::kOpUMinus: {
                exp->arg->Accept(this);
                auto val = res;
                res = std::make_shared<ir::TmpVar>(
                    AluSelectType(ir::t_i32, val->ty));
                bb->insts.push_back(std::make_unique<ir::BinaryAlu>(
                    ir::Instr::kOpSub, res->ty, m.CreateImm(0),
                    AutoTypeCast(res->ty, val), res));
            } break;
            case ast::AstNode::kOpUAdd:
                visit_child = true;
                break;
            }
        }
        return visit_child;
    }

    bool VisitExp(ast::AstNode *exp) {
        if (exp->IsBinaryExp()) {
            return VisitBinaryExp(exp->dyn<ast::BinaryExp>());
        } else if (exp->IsUnaryExp()) {
            return VisitUnaryExp(exp->dyn<ast::UnaryExp>());
        }
        switch (exp->op) {
        case ast::AstNode::kOpConst: {
            auto c = exp->dyn<ast::ConstExp>();
            res = m.CreateImm(c->val);
        } break;
        case ast::AstNode::kOpVar: {
            auto ast_var = exp->dyn<ast::VarExp>();
            auto ast_decl = current_scope->Find(ast_var->name);
            auto var = ast_ir_decl_map[ast_decl];
            res = var;
            if (std::dynamic_pointer_cast<ir::PtrT>(var->ty)->p->kind ==
                ir::Type::kArray) {
                ReferArray(dynamic_cast<ast::ArrayDecl *>(ast_decl), nullptr);
            } else {
                LoadIfNeed(var);
            }
        } break;
        case ast::AstNode::kOpCall: {
            auto ast_call = exp->dyn<ast::CallExp>();
            auto call = std::make_unique<ir::Call>();
            call->func = std::dynamic_pointer_cast<ir::GlobalVar>(
                m.FindVar(AstFuncNameToIr(ast_call->name)));
            auto ir_ft = std::dynamic_pointer_cast<ir::FuncT>(call->func->ty);
            if (ir_ft->ret->kind != ir::Type::kVoid)
                call->result = std::make_shared<ir::TmpVar>(ir_ft->ret);

            for (auto x : ast_call->args) {
                varctx_stack.Do([&](VarCtx &ctx) {
                    ctx.need_load = true;
                    cond_stack.Do([&](CondCtx &ctx) {
                        ctx.active = false;
                        x->Accept(this);
                    });
                    call->args.push_back(res);
                });
            }
            res = call->result;
            bb->insts.push_back(std::move(call));
        } break;
        case ast::AstNode::kOpRef: {
            auto ref = exp->dyn<ast::RefExp>();
            auto ast_decl =
                current_scope->Find(ref->name)->dyn<ast::ArrayDecl>();
            ReferArray(ast_decl, ref);
        } break;
        }
        LinkCondIfNeed();
        return false;
    }

    bool VisitStmt(ast::Stmt *_stmt) {
        // default return true (means that continue visiting chidren)
        switch (_stmt->op) {
        case ast::AstNode::kOpBlock: {
            EnterScope(_stmt->dyn<ast::BlockStmt>()->id);
            for (auto st : _stmt->dyn<ast::BlockStmt>()->stmts) {
                if (!bb)
                    break;
                st->Accept(this);
            }
            LeaveScope();
            return false;
        }
        // Nop
        case ast::AstNode::kOpAssign: {
            auto stmt = _stmt->dyn<ast::AssignStmt>();
            varctx_stack.Do([&](VarCtx &ctx) {
                ctx.need_load = false;
                stmt->lval->Accept(this);
            });
            assert(res->ty->kind == ir::Type::kPtr);
            auto ptr_ty = std::dynamic_pointer_cast<ir::PtrT>(res->ty);
            auto ptr = res;
            varctx_stack.Do([&](VarCtx &ctx) {
                ctx.need_load = true;
                stmt->rval->Accept(this);
            });
            auto val = res;
            val = AutoTypeCast(ptr_ty->p, val);
            bb->insts.push_back(std::make_unique<ir::Store>(val, ptr));
            return false;
        }
        case ast::AstNode::kOpDeclStmt: {
            auto decl_stmt = _stmt->dyn<ast::DeclStmt>();
            for (auto decl : decl_stmt->decls) {
                current_scope->Insert(decl);
                // WARN: only support i32 and i32 array, limited by ast
                auto ty = AstType2IrType(decl->type);
                // WARN: alignment hard-coded as 4, but should be set
                // according to type
                // TODO: detect alignment
                // pv = alloca <ty>
                auto ptr_ty = std::make_shared<ir::PtrT>(ty);
                res =
                    ir_func->CreateLocalVar(ptr_ty, var_prefix.Var(decl->name));
                ast_ir_decl_map[decl] = res;
                auto ptr = res;
                // bb->insts.push_back(std::make_unique<ir::Alloca>(ty, res,
                // 4));
                res = Alloca(ty, res);
                if (decl->HasInitVal()) {
                    if (decl->op == ast::AstNode::kOpArrayDecl) {
                        auto one_dim_ty = AstArrayTy2OneDimIrArrayTy(
                            dynamic_cast<ast::ArrayT *>(decl->type));
                        ptr = AutoTypeCast(
                            std::make_shared<ir::PtrT>(one_dim_ty), ptr);
                        SetI32ArrayZero(ptr, one_dim_ty->count);
                        auto arr = decl->dyn<ast::ArrayDecl>();
                        for (auto record : arr->init_val_records) {
                            int idx = record.pos;
                            for (auto x : record.vals) {
                                varctx_stack.Do([&](VarCtx &ctx) {
                                    ctx.need_load = true;
                                    x->Accept(this);
                                });
                                auto init_val = res;
                                auto init_ptr = ir_func->CreateTmpVar(
                                    std::make_shared<ir::PtrT>(
                                        one_dim_ty->element));
                                bb->insts.push_back(
                                    std::make_unique<ir::Getelementptr>(
                                        one_dim_ty, ptr,
                                        std::vector<std::shared_ptr<ir::Value>>{
                                            m.CreateImm(0), m.CreateImm(idx)},
                                        init_ptr));
                                bb->insts.push_back(std::make_unique<ir::Store>(
                                    init_val, init_ptr));
                                idx++;
                            }
                        }
                    } else if (decl->op == ast::AstNode::kOpBDecl) {
                        auto bdecl = decl->dyn<ast::BDecl>();
                        varctx_stack.Do([&](VarCtx &ctx) {
                            ctx.need_load = true;
                            bdecl->init->Accept(this);
                        });
                        bb->insts.push_back(
                            std::make_unique<ir::Store>(res, ptr));
                    }
                }
                if (decl->op == ast::AstNode::kOpFuncDecl) {
                    auto func_decl = decl->dyn<ast::FuncDecl>();
                    AddFuncDecl(func_decl);
                }
            }
            return false;
        }
        case ast::AstNode::kOpWhile: {
            auto stmt = _stmt->dyn<ast::WhileStmt>();
            while_stack.Do([&](WhileCtx &ctx) {
                ctx.exit = ir_func->CreateTmpVar(ir::t_label);
                ctx.body = ir_func->CreateTmpVar(ir::t_label);
                ctx.header = ir_func->CreateTmpVar(ir::t_label);
                ir::BB *header = ir_func->CreateBB();
                header->label = ctx.header;
                ir_func->label_map[ctx.header.get()] = header;

                // -> header
                bb->insts.push_back(std::make_unique<ir::Br>(ctx.header));
                bb = header;
                bb->tag = ir::BB::kLoopHeader;

                //  header
                //  /   \
                // body _exit
                cond_stack.Do([&](CondCtx &condctx) {
                    condctx.tlabel = ctx.body;
                    condctx.flabel = ctx.exit;
                    stmt->cond->Accept(this);
                });

                // body
                auto body = ir_func->CreateBB();
                body->label = ctx.body;
                ir_func->label_map[ctx.body.get()] = body;
                bb = body;
                bb->tag = ir::BB::kLoopBody;
                // body -> header
                stmt->stmt->Accept(this);
                if (bb) {
                    bb->insts.push_back(std::make_unique<ir::Br>(ctx.header));
                }

                // create next bb
                bb = ir_func->CreateBB();
                bb->label = ctx.exit;
                ir_func->label_map[ctx.exit.get()] = bb;
            });
            return false;
        }
        case ast::AstNode::kOpBreak: {
            assert(while_stack.Current().active);
            bb->insts.push_back(
                std::make_unique<ir::Br>(while_stack.Current().exit));
            bb = nullptr;
            return false;
        }
        case ast::AstNode::kOpContinue: {
            assert(while_stack.Current().active);
            bb->insts.push_back(
                std::make_unique<ir::Br>(while_stack.Current().header));
            bb = nullptr;
            return false;
        }
        case ast::AstNode::kOpIf: {
            auto stmt = _stmt->dyn<ast::IfStmt>();

            std::shared_ptr<ir::LocalValue> l1 = ir_func->CreateTmpVar(
                                                ir::t_label),
                                            next = ir_func->CreateTmpVar(
                                                ir::t_label),
                                            l2 = next;
            if (stmt->fstmt)
                l2 = ir_func->CreateTmpVar(ir::t_label);

            // cond
            cond_stack.Do([&](CondCtx &ctx) {
                ctx.tlabel = l1;
                ctx.flabel = l2;
                stmt->cond->Accept(this);
            });

            // if then
            auto tbb = ir_func->CreateBB();
            tbb->label = l1;
            ir_func->label_map[l1.get()] = tbb;
            bb = tbb;
            stmt->tstmt->Accept(this);
            auto tbb_end = bb;
            int no_next = 0;

            // if else
            if (stmt->fstmt) {
                auto fbb = ir_func->CreateBB();
                fbb->label = l2;
                ir_func->label_map[l2.get()] = fbb;
                bb = fbb;
                stmt->fstmt->Accept(this);
                // if else -> next
                if (bb) {
                    bb->insts.push_back(std::make_unique<ir::Br>(next));
                } else
                    no_next++;
            }

            // if then -> next
            if (tbb_end) {
                tbb_end->insts.push_back(std::make_unique<ir::Br>(next));
            } else
                no_next++;

            if (no_next != 2) {
                bb = ir_func->CreateBB();
                bb->label = next;
                ir_func->label_map[next.get()] = bb;
            } else {
                bb = nullptr;
            }

            return false;
        }
        // ExpStmt // visit children
        case ast::AstNode::kOpReturn: {
            auto ret_ty = ir_func->ft->ret;
            auto retexp = _stmt->dyn<ast::ReturnStmt>()->val;
            std::shared_ptr<ir::Value> retval;
            if (!ret_ty->eq(*ir::t_void) && retexp) {
                retexp->Accept(this);
                retval = res;
            }
            bb->insts.push_back(std::make_unique<ir::Ret>(ret_ty, retval));
            bb = nullptr;
            return false;
        }
        }
        return true;
    }

    virtual bool Visit(ast::AstNode *node) {
        VisitChildBeforeRoutine();
        if (node->IsStmt()) {
            return VisitStmt(node->dyn<ast::Stmt>());
        } else if (node->IsExp()) {
            return VisitExp(node->dyn<ast::Exp>());
        }
        return true;
    }

  private:
    std::shared_ptr<ir::Type> GetElementType(std::shared_ptr<ir::Type> ty) {
        if (ty->kind == ir::Type::kPtr) {
            return ty->cast<ir::PtrT>()->p;
        } else if (ty->kind == ir::Type::kArray) {
            return ty->cast<ir::ArrayT>()->element;
        } else {
            assert(false);
        }
    }

    std::shared_ptr<ir::Value> ReferArrayOne(std::shared_ptr<ir::Value> arr,
                                             std::shared_ptr<ir::Value> ref) {
        auto next_ty =
            [&](std::shared_ptr<ir::PtrT> ty) -> std::shared_ptr<ir::PtrT> {
            auto p = ty->p;
            if (p->kind != ir::Type::kPtr) {
                return std::make_shared<ir::PtrT>(GetElementType(p));
            } else {
                return std::dynamic_pointer_cast<ir::PtrT>(p);
            }
        };

        std::shared_ptr<ir::Value> res;

        auto arr_ptr_ty = std::dynamic_pointer_cast<ir::PtrT>(arr->ty);
        if (arr_ptr_ty->p->kind == ir::Type::kArray) {
            res = ir_func->CreateTmpVar(next_ty(arr_ptr_ty));
            bb->insts.push_back(std::make_unique<ir::Getelementptr>(
                arr_ptr_ty->p, arr,
                std::vector<std::shared_ptr<ir::Value>>{m.CreateImm(0), ref},
                res));
        } else {
            assert(arr_ptr_ty->p->kind == ir::Type::kPtr);
            auto ptr = ir_func->CreateTmpVar(arr_ptr_ty->p);
            bb->insts.push_back(std::make_unique<ir::Load>(ptr->ty, arr, ptr));
            res = ir_func->CreateTmpVar(arr_ptr_ty->p);
            bb->insts.push_back(std::make_unique<ir::Getelementptr>(
                std::dynamic_pointer_cast<ir::PtrT>(ptr->ty)->p, ptr,
                std::vector<std::shared_ptr<ir::Value>>{ref}, res));
        }

        return res;
    }

    void ReferArray(ast::ArrayDecl *ast_decl, ast::RefExp *ref) {
        // a[2][3]:  a / &a[0], a[1] / &a[1][0], a[1][2]
        // a[][3]:  a / &a[0], a[1] / &a[1][0], a[1][2]
        // a[]: a / &a[0], a[1]
        // directly referring a is handled by LoadVar.
        auto arr = ast_ir_decl_map[ast_decl];
        assert(arr);
        auto ast_arr_ty = ast_decl->GetType();
        if (arr->kind == ir::Value::kGlobalVar) {
            auto ir_ty = ast_array_type_map[arr.get()];
            arr = AutoTypeCast(ir_ty, arr);
        }

        int ref_count = 0;
        if (ref) {
            for (auto x : ref->ref->dimensions) {
                varctx_stack.Do([&](VarCtx &ctx) {
                    ctx.need_load = true;
                    cond_stack.Do([&](CondCtx &ctx) {
                        ctx.active = false;
                        x->Accept(this);
                    });
                });
                arr = ReferArrayOne(arr, res);
                ref_count++;
            }
        }

        bool can_not_load = false;
        if (ref_count < ast_arr_ty->dimensions.size()) {
            // &a[0]
            arr = ReferArrayOne(arr, m.CreateImm(0));
            can_not_load = true;
        }
        res = arr;

        if (!can_not_load &&
            std::dynamic_pointer_cast<ir::PtrT>(arr->ty)->p->kind !=
                ir::Type::kArray) {
            LoadIfNeed(arr);
        }
    }

    void LinkCondIfNeed() {
        if (cond_stack.IsActive()) {
            if (res->ty->kind != ir::Type::kI1) {
                auto l = res;
                res = std::make_shared<ir::TmpVar>(ir::t_i1);
                bb->insts.push_back(std::make_unique<ir::Icmp>(
                    ir::Icmp::kNe, AluSelectType(l->ty, ir::t_i32), l,
                    m.CreateImm(0), res));
            }
            bb->insts.push_back(std::make_unique<ir::Br>(
                cond_stack.Current().tlabel, res, cond_stack.Current().flabel));
            bb = nullptr;
        }
    }

    void LoadIfNeed(std::shared_ptr<ir::Value> var) {
        if (varctx_stack.IsActive() && varctx_stack.Current().need_load) {
            LoadVar(var);
        }
    }

    void LoadVar(std::shared_ptr<ir::Value> var) {
        assert(var->ty->kind == ir::Type::kPtr);
        auto ptr_ty = std::dynamic_pointer_cast<ir::PtrT>(var->ty);
        assert(ptr_ty->p->kind != ir::Type::kArray);
        res = ir_func->CreateTmpVar(ptr_ty->p);
        bb->insts.push_back(std::make_unique<ir::Load>(ptr_ty->p, var, res));
    }

    // check val type, if type is not expected, cast val to expected
    std::shared_ptr<ir::Value> AutoTypeCast(std::shared_ptr<ir::Type> expected,
                                            std::shared_ptr<ir::Value> val) {
        if (expected && val->ty->eq(*expected))
            return val;
        std::shared_ptr<ir::Value> res;
        // ptr -> ptr, use bitcast directly
        if (val->ty->kind == ir::Type::kPtr &&
            expected->kind == ir::Type::kPtr) {
            res = ir_func->CreateTmpVar(expected);
            bb->insts.push_back(
                std::make_unique<ir::Bitcast>(expected, val, res));
            return res;
        }
        // i1 -> i32
        if (val->ty->eq(*ir::t_i1) && expected->eq(*ir::t_i32)) {
            res = ir_func->CreateTmpVar(expected);
            bb->insts.push_back(std::make_unique<ir::Zext>(expected, val, res));
            return res;
        }

        fprintf(stderr, "convert from %s to %s\n", val->ty->str().c_str(),
                expected->str().c_str());
        assert(false && "type cast not supported");
    }

    std::shared_ptr<ir::Type> AluSelectType(std::shared_ptr<ir::Type> l,
                                            std::shared_ptr<ir::Type> r) {
        std::shared_ptr<ir::Type> ty;
        // ==
        if (l->eq(*r))
            return l;

        // i1 -> i32
        ty = ir::t_i32;
        auto check_i1_i32 = [&](std::shared_ptr<ir::Type> i1,
                                std::shared_ptr<ir::Type> i32) {
            if (i1->eq(*ir::t_i1) && i32->eq(*ir::t_i32)) {
                return true;
            }
            return false;
        };
        if (check_i1_i32(l, r) || check_i1_i32(r, l))
            return ty;

        fprintf(stderr, "AluSelectType: %s, %s\n", l->str().c_str(),
                r->str().c_str());
        assert(false);
        return ty;
    }

    ir::FuncDecl *AstFuncDecl2IrFuncDecl(ast::FuncDecl *decl) {
        std::string name = AstFuncNameToIr(decl->name);
        auto ir_func_decl = new ir::FuncDecl();
        ir_func_decl->name = name;
        ir_func_decl->ft =
            std::dynamic_pointer_cast<ir::FuncT>(AstType2IrType(&decl->ft));
        return ir_func_decl;
    }

    void AddFuncDecl(ast::FuncDecl *ast_decl, std::string name = "") {
        if (ast_decl == nullptr)
            ast_decl = ast_builtin_funcs[name];
        auto decl = AstFuncDecl2IrFuncDecl(ast_decl);
        m.InsertFuncDecl(decl);
    }

    std::string AstFuncNameToIr(std::string name) {
        auto it_name = link_name_map.find(name);
        if (it_name != link_name_map.end())
            return it_name->second;
        return name;
    }

    template <typename T>
    std::shared_ptr<T> Alloca(std::shared_ptr<ir::Type> ty,
                              std::shared_ptr<T> res = {}, int align = 4) {
        if (!res)
            res = ir_func->CreateTmpVar(std::make_shared<ir::PtrT>(ty));
        ir_func->bblocks[0]->insts.push_back(
            std::make_unique<ir::Alloca>(ty, res, align));
        return res;
    }

    void HandleDecls() {
        for (auto decl : comp_unit->decls) {
            switch (decl->op) {
            case ast::AstNode::kOpBDecl: {
                auto def = std::make_unique<ir::Define>();
                auto ty = AstType2IrType(decl->type);
                auto v = m.CreateGlobalVar(std::make_shared<ir::PtrT>(ty),
                                           decl->name);
                ast_ir_decl_map[decl] = v;
                def->is_const = decl->is_const;
                def->var = v;
                auto init = std::make_unique<ir::BasicInit>();
                int init_val = 0;
                if (decl->HasInitVal())
                    init_val = decl->dyn<ast::BDecl>()->init->eval({});
                init->val = m.CreateImm(init_val, ty);
                def->init = std::move(init);
                m.defs.push_back(std::move(def));
            } break;
            case ast::AstNode::kOpArrayDecl: {
                auto def = std::make_unique<ir::Define>();
                auto arr_decl = dynamic_cast<ast::ArrayDecl *>(decl);
                // eg: int [2][3] -> int [6]
                auto ty = AstArrayTy2OneDimIrArrayTy(
                    dynamic_cast<ast::ArrayT *>(decl->type));
                // eg: int (*)[6]
                auto p_ty = std::make_shared<ir::PtrT>(ty);
                auto v = m.CreateGlobalVar(p_ty, decl->name);
                ast_ir_decl_map[decl] = v;
                ast_array_type_map[v.get()] =
                    std::make_shared<ir::PtrT>(AstType2IrType(decl->type));
                def->var = v;
                def->is_const = decl->is_const;
                if (decl->HasInitVal()) {
                    auto init = std::make_unique<ir::ArrayInit>();
                    init->ty = ty;
                    for (auto &record : arr_decl->init_val_records) {
                        init->vals.resize(record.pos);
                        for (auto x : record.vals) {
                            init->vals.push_back(
                                std::make_unique<ir::BasicInit>(
                                    m.CreateImm(x->eval({}), ty->element)));
                        }
                    }
                    init->vals.resize(ty->count);
                    def->init = std::move(init);
                } else {
                    def->init = std::make_unique<ir::Zeroinitializer>(ty);
                }
                m.defs.push_back(std::move(def));
            } break;
            case ast::AstNode::kOpFuncDecl: {
                AddFuncDecl(dynamic_cast<ast::FuncDecl *>(decl));
            } break;
            default:
                assert(false && "type can not be defined in global");
            }
            current_scope->Insert(decl);
        }
    }

    void HandleFuncs() {
        // create function define, but do not create function body
        for (auto f : comp_unit->funcs) {
            auto func = new ir::Func;
            // !!! MAY BUG !!!
            func->m = &m;
            func->name = f->name;
            func->ft =
                std::dynamic_pointer_cast<ir::FuncT>(AstType2IrType(&f->ft));
            for (int i = 0; i < f->args.size(); i++) {
                func->args.push_back(
                    func->CreateLocalVar(func->ft->args[i], f->args[i]->name));
            }
            m.InsertFunc(func);
        }

        // create function body
        for (int i = 0; i < comp_unit->funcs.size(); i++) {
            ast_func = comp_unit->funcs[i];
            ir_func = m.funcs[i].get();
            bb = ir_func->CreateBB();
            var_prefix.PushPrefix(ir_func->name);
            // create function args
            EnterScope(0);
            for (int i = 0; i < ir_func->args.size(); i++) {
                auto arg = ir_func->args[i];
                auto ptr_ty = std::make_shared<ir::PtrT>(arg->ty);
                res =
                    ir_func->CreateLocalVar(ptr_ty, var_prefix.Var(arg->var()));
                current_scope->Insert(ast_func->args[i]);
                ast_ir_decl_map[ast_func->args[i]] = res;
                res = Alloca<ir::Value>(arg->ty, res);
                // bb->insts.push_back(std::make_unique<ir::Alloca>(arg->ty,
                // res));
                bb->insts.push_back(std::make_unique<ir::Store>(arg, res));
            }
            auto next = ir_func->CreateTmpVar(ir::t_label);
            bb = ir_func->CreateBB();
            bb->label = next;
            ir_func->label_map[next.get()] = bb;
            ast_func->block->Accept(this);
            var_prefix.PopPrefix();
            if (bb && ir_func->ft->ret->eq(*ir::t_void)) {
                // for endless loop: while (1) {}
                // assert(ir_func->ft->ret->eq(*ir::t_void));
                bb->insts.push_back(std::make_unique<ir::Ret>(ir::t_void));
                bb = nullptr;
            }
            if (ir_func->bblocks.back()->insts.size() == 0)
                ir_func->bblocks.back()->insts.push_back(
                    std::make_unique<ir::Br>(ir_func->bblocks.back()->label));
            if (ir_func->bblocks[0]->insts.size() == 0) {
                ir_func->bblocks.erase(ir_func->bblocks.begin());
            } else {
                ir_func->bblocks[0]->insts.push_back(
                    std::make_unique<ir::Br>(next));
            }
            LeaveScope();
        }
    }
};

ir::Module *AstToIr(ast::CompUnit *comp_unit) {
    ir::Module *m = new ir::Module();
    IrBuildVisitor v(*m, comp_unit);
    v.Build();
    return m;
}
