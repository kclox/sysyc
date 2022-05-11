
#include "ast/ast.hpp"
#include "error.h"
#include <stack>

using namespace ast;

static const char *type_names[] = {"undefine", "int", "void", "array"};
#define TYPE_COUNT (sizeof(type_names) / sizeof(const char *))

static const std::string op_names[] = {
    "kOpBDecl",  "kOpArrayDecl", "kOpFuncDecl", "kOpFunc",   "kOpNop",
    "kOpAssign", "kOpDeclStmt",  "kOpWhile",    "kOpBreak",  "kOpContinue",
    "kOpIf",     "kOpBlock",     "kOpExpStmt",  "kOpReturn", "kOpConst",
    "kOpVar",    "kOpCall",      "kOpRef",      "kOpAdd",    "kOpSub",
    "kOpMul",    "kOpDiv",       "kOpMod",      "kOpGt",     "kOpLt",
    "kOpEq",     "kOpNe",        "kOpGe",       "kOpLe",     "kOpLAnd",
    "kOpLOr",    "kOpNot",       "kOpUMinus",   "kOpUAdd",   "kOpArrayInit"};
#define OP_COUNT (sizeof(op_names) / sizeof(const char *))

const char *ast::Type2Str(int type) {
    if (type < TYPE_COUNT)
        return type_names[type];
    return "unknown";
}

void Visitor::VisitChildBefore(AstNode *node) {
    visit_child_before = true;
    node->Accept(this);
}

const std::string AstNode::Op2Str(int op) {
    if (op < OP_COUNT)
        return op_names[op];
    return "Unknown";
}

Exp *BDecl::InitVal() {
    if (HasInitVal()) {
        return init;
    }
    return ConstExp::zero;
}

json_t BDecl::to_json() const {
    auto _json = Decl::to_json();
    // _json["global_init_evaluated"] = global_init_evaluated;
    _json["init"] = nullptr;
    if (HasInitVal())
        _json["init"] = init->to_json();
    return _json;
}

void BDecl::EvalGlobalInit(Scope<Decl *> *scope) {
    if (!global_init_evaluated) {
        init = new ConstExp(init->eval({.scope = scope}));
        global_init_evaluated = true;
    }
}

void BDecl::Accept(Visitor *visitor) {
    if (visitor->Visit(this) && HasInitVal()) {
        init->Accept(visitor);
    }
}

json_t ArrayDecl::to_json() const {
    auto _json = Decl::to_json();
    auto type_to_json = [&]() {
        if (array_type_maturity == 0)
            return json_t(
                {{"element", type->to_json()}, {"count", array->to_json()}});
        else
            return type->to_json();
    };
    // _json["array_type_maturity"] = array_type_maturity;
    _json["type"] = type_to_json();
    // _json["global_init_evaluated"] = global_init_evaluated;
    _json["init"] = nullptr;
    if (HasInitVal())
        _json["init"] = init->to_json();
    if (array_init_record_set) {
        _json["init_val_records"] = {};
        for (auto x : init_val_records)
            _json["init_val_records"].push_back(x);
    }
    return _json;
}

Exp *ArrayDecl::GetInitVal(int idx) {
    Exp *ret = ConstExp::zero;
    for (auto &record : init_val_records) {
        if (record.pos > idx)
            break;
        else {
            int i = idx - record.pos;
            if (i < record.vals.size())
                ret = record.vals[i];
        }
    }
    return ret;
}

void ArrayDecl::InitArrayType(Scope<Decl *> *scope, bool is_func_arg) {
    if (array_type_maturity == 0) {
        // set type
        // whne not inited, type is element type
        auto array_ty = g_type_system.TArray(this->type);
        bool first_element = true;
        for (auto x : array->dimensions) {
            if (!x) {
                if (!first_element)
                    throw SyntaxError("an array may not have elements "
                                      "of this type");
                if (HasInitVal())
                    array_ty->dimensions.push_back(0);
                else if (is_func_arg)
                    array_ty->dimensions.push_back(0);
                else
                    throw SyntaxError("incomplete type is not allowed");
            } else {
                // check exp is const or not
                array_ty->dimensions.push_back(
                    x->eval({.scope = scope, .must_const = true}));
            }
            first_element = false;
        }
        this->type = array_ty;
        array_type_maturity = 1;
        // NOTE: now the array_ty's first dim is not set
        // we need check its init's structure
    }
}

void ArrayDecl::EvalGlobalInit(Scope<Decl *> *scope) {
    if (!global_init_evaluated) {
        std::function<void(ArrayInitVal *)> eval_init;
        eval_init = [&](ArrayInitVal *init) {
            for (auto &x : init->children) {
                if (x->kind == ArrayInitItem::kList)
                    eval_init(dynamic_cast<ArrayInitVal *>(x));
                else if (x->kind == ArrayInitItem::kExp)
                    x = new ConstExp(
                        dynamic_cast<Exp *>(x)->eval({.scope = scope}));
            }
        };
        eval_init(init);
        global_init_evaluated = true;
    }
}

void ArrayDecl::InitArrayInitRecords(bool is_func_arg) {
    auto array_ty = GetType();
    if (!array_init_record_set) {
        // for array: [][2][3][2]
        // -> [0][12][6][2]
        std::vector<int> count_list = array_ty->dimensions;
        for (auto it = count_list.rbegin() + 1; it != count_list.rend(); it++)
            *it *= *(it - 1);

        auto get_off_level = [&](int level, int off) -> int {
            for (int i = level + 1; i < count_list.size(); i++) {
                if (off % count_list[i] == 0)
                    return i - 1;
            }
            return count_list.size() - 1;
        };
        auto get_child_count = [&](int level) -> int {
            if (level == count_list.size() - 1)
                return 1;
            return count_list[level + 1];
        };

        // innermost list
        auto innermost_list = [&](ArrayInitItem *arr) -> ArrayInitVal * {
            ArrayInitItem *p = arr;
            ArrayInitVal *prev;
            while (p->kind == ArrayInitItem::kList) {
                auto arr_p = dynamic_cast<ArrayInitVal *>(p);
                if (arr_p->children.size() > 1 || arr_p->children.size() == 0)
                    return arr_p;
                p = arr_p->children.front();
                prev = arr_p;
            }
            return prev;
        };

        if (HasInitVal() && !is_func_arg) {
            // set init_val_records
            std::function<int(ast::ArrayInitVal * init, int level, int pos)>
                set_array_init_val;
            set_array_init_val = [&](ArrayInitVal *init, int level,
                                     int base) -> int {
                int consumed = 0;
                if (level >= array_ty->dimensions.size()) {
                    level = array_ty->dimensions.size() - 1;
                }

                bool pos_set = false; // pos is set or not
                ArrayInitRecord record;
                int off = 0;

                auto set_exp_init = [&](Exp *exp) {
                    if (!pos_set) {
                        record.pos = base + off;
                        pos_set = true;
                    }
                    if (count_list[level] != 0 && off >= count_list[level])
                        throw SyntaxError("too many initializer values");
                    record.vals.push_back(exp);
                    off++;
                    if (off % get_child_count(level) == 0)
                        consumed++;
                };

                if (init->children.size() > 0) {
                    for (auto x : init->children) {
                        if (x->kind == ArrayInitItem::kList) {
                            auto arr_x = static_cast<ArrayInitVal *>(x);
                            int next_level = get_off_level(level, off);
                            int child_count = get_child_count(next_level);
                            if (child_count == 1) {
                                auto arr = innermost_list(arr_x);
                                int size = arr->children.size();
                                if (size == 0)
                                    continue;
                                else if (size == 1) {
                                    auto p = arr->children.front();
                                    set_exp_init(dynamic_cast<Exp *>(p));
                                } else {
                                    throw SyntaxError(
                                        "too many initializer values");
                                }

                            } else {
                                if (record.vals.size() > 0) {
                                    this->init_val_records.push_back(record);
                                    record.Reset();
                                    pos_set = false;
                                }
                                set_array_init_val(arr_x, next_level,
                                                   base + off);
                                off += child_count;
                                if (off % get_child_count(level) == 0)
                                    consumed++;
                            }
                        } else if (x->kind == ArrayInitItem::kExp) {
                            set_exp_init(dynamic_cast<Exp *>(x));
                        } else {
                            assert(false && "ArrayInitItem kind not supported");
                        }
                    }
                    if (record.vals.size() > 0) {
                        this->init_val_records.push_back(record);
                        if (off % get_child_count(level) != 0)
                            consumed++;
                    }
                }
                return consumed;
            };
            int init_count = set_array_init_val(init, 0, 0);
            if (array_ty->dimensions[0] == 0)
                array_ty->dimensions[0] = init_count;
            array_type_maturity = 2;
            array_init_record_set = true;
        }
    }
}

void ArrayDecl::Accept(Visitor *visitor) {
    if (visitor->Visit(this)) {
        array->VisitDimExps(visitor);
        if (HasInitVal()) {
            std::function<void(ast::ArrayInitVal *)> accpet;
            accpet = [&](ArrayInitVal *p) {
                for (auto x : p->children) {
                    switch (x->kind) {
                    case ArrayInitItem::kExp:
                        x->Accept(visitor);
                        break;
                    case ArrayInitItem::kList:
                        accpet(static_cast<ArrayInitVal *>(x));
                        break;
                    default:
                        assert(false && "kind not supported");
                    }
                }
            };
            accpet(init);
        }
    }
}

void AssignStmt::Accept(Visitor *visitor) {
    if (visitor->Visit(this)) {
        lval->Accept(visitor);
        rval->Accept(visitor);
    }
}

json_t AssignStmt::to_json() const {
    auto j = Stmt::to_json();
    j["lval"] = lval->to_json();
    j["rval"] = rval->to_json();
    return j;
}

void DeclStmt::Accept(Visitor *visitor) {
    if (visitor->Visit(this))
        for (auto decl : decls)
            decl->Accept(visitor);
}

json_t DeclStmt::to_json() const {
    json_t j = Stmt::to_json();
    j["decls"] = {};
    for (auto decl : decls) {
        j["decls"].push_back(decl->to_json());
    }
    return j;
}

json_t WhileStmt::to_json() const {
    auto j = Stmt::to_json();
    j["cond"] = cond->to_json();
    j["stmt"] = stmt->to_json();
    return j;
}

void WhileStmt::Accept(Visitor *visitor) {
    if (visitor->Visit(this)) {
        this->cond->Accept(visitor);
        this->stmt->Accept(visitor);
    }
}

json_t IfStmt::to_json() const {
    auto j = Stmt::to_json();
    j["cond"] = cond->to_json();
    j["tstmt"] = tstmt->to_json();
    j["fstmt"] = fstmt ? fstmt->to_json() : json_t(nullptr);
    return j;
}

void IfStmt::Accept(Visitor *visitor) {
    if (visitor->Visit(this)) {
        cond->Accept(visitor);
        tstmt->Accept(visitor);
        if (fstmt)
            fstmt->Accept(visitor);
    }
}

json_t BlockStmt::to_json() const {
    auto j = Stmt::to_json();
    j["stmts"] = json_t(R"([])"_json);
    for (auto x : stmts)
        j["stmts"].push_back(x->to_json());
    return j;
}

void BlockStmt::Accept(Visitor *visitor) {
    if (visitor->Visit(this))
        for (auto stmt : stmts)
            stmt->Accept(visitor);
}

void BlockStmt::SetBlockID() {
    if (func)
        id = func->AddBlock();
}

json_t ExpStmt::to_json() const {
    auto j = Stmt::to_json();
    j["exp"] = exp->to_json();
    return j;
}

void ExpStmt::Accept(Visitor *visitor) {
    if (visitor->Visit(this))
        this->exp->Accept(visitor);
}

json_t ReturnStmt::to_json() const {
    auto j = Stmt::to_json();
    j["val"] = val ? val->to_json() : json_t(nullptr);
    return j;
}

void ReturnStmt::Accept(Visitor *visitor) {
    if (visitor->Visit(this) && val)
        val->Accept(visitor);
}

json_t Func::to_json() const {
    auto j = AstNode::to_json();
    json_t args;
    for (auto x : this->args) {
        args.push_back(x->to_json());
    }
    j.merge_patch({{"name", name},
                   {"ret", ft.ret->to_json()},
                   {"args", args},
                   {"block", block->to_json()}});
    return j;
}

void Func::Accept(Visitor *visitor) {
    if (visitor->Visit(this)) {
        for (auto arg : args)
            arg->Accept(visitor);
        block->Accept(visitor);
    }
}

json_t ConstExp::to_json() const {
    auto j = Exp::to_json();
    j["val"] = val;
    return j;
}

ConstExp *ConstExp::zero = new ConstExp(0);

int VarExp::eval(EvalParams params) {
    assert(params.scope != nullptr && "scope can not be nullptr");
    auto decl = params.scope->Find(name);
    assert(decl != nullptr && "use before define");
    assert(decl->op == kOpBDecl && "ArrayDecl and FuncDecl is not evaluable");
    if (params.must_const && !decl->IsConst())
        assert(false && "decl must be const");
    auto init = decl->dyn<BDecl>()->InitVal();
    assert(init->op == kOpConst);
    return init->dyn<ConstExp>()->val;
}

json_t VarExp::to_json() const {
    auto j = AstNode::to_json();
    j["name"] = name;
    return j;
}

json_t CallExp::to_json() const {
    auto j = Exp::to_json();
    j["name"] = name;
    j["args"] = {};
    for (auto x : args)
        j["args"].push_back(x->to_json());
    return j;
}

void CallExp::Accept(Visitor *visitor) {
    if (visitor->Visit(this)) {
        for (auto arg : args) {
            arg->Accept(visitor);
        }
    }
}

json_t RefExp::to_json() const {
    auto j = Exp::to_json();
    j["name"] = name;
    j["ref"] = ref->to_json();
    return j;
}

int RefExp::eval(EvalParams params) {
    assert(params.scope != nullptr && "scope can not be nullptr");
    auto decl = params.scope->Find(name);
    assert(decl != nullptr && "use before define");
    auto array_decl = decl->dyn<ArrayDecl>();
    if (!array_decl->HasInitVal())
        return 0;
    auto array_ty = array_decl->GetType();
    int base = array_ty->Count(), i = 0, idx = 0;
    for (auto x : ref->dimensions) {
        base /= array_ty->dimensions[i];
        idx += x->eval(params) * base;
        i++;
    }
    auto init = array_decl->GetInitVal(idx);
    assert(init->op == kOpConst);
    return init->dyn<ConstExp>()->val;
}

void RefExp::Accept(Visitor *visitor) {
    if (visitor->Visit(this)) {
        ref->VisitDimExps(visitor);
    }
}

json_t UnaryExp::to_json() const {
    auto j = Exp::to_json();
    j["arg"] = arg->to_json();
    return j;
}

int UnaryExp::eval(EvalParams params) {
    switch (op) {
    case kOpNot:
        return !arg->eval(params);
    case kOpUMinus:
        return -arg->eval(params);
    case kOpUAdd:
        return arg->eval(params);
    }
    assert(false);
    return 0;
}

void UnaryExp::Accept(Visitor *visitor) {
    if (visitor->Visit(this))
        arg->Accept(visitor);
}

json_t BinaryExp::to_json() const {
    auto j = Exp::to_json();
    j.merge_patch({{"left", left->to_json()}, {"right", right->to_json()}});
    return j;
}

int BinaryExp::eval(EvalParams params) {
    int l = left->eval(params), r = right->eval(params);
    switch (op) {
    case kOpAdd:
        return l + r;
    case kOpSub:
        return l - r;
    case kOpMul:
        return l * r;
    case kOpDiv:
        return l / r;
    case kOpMod:
        return l % r;
    case kOpGt:
        return l > r;
    case kOpLt:
        return l > r;
    case kOpEq:
        return l == r;
    case kOpNe:
        return l != r;
    case kOpGe:
        return l >= r;
    case kOpLe:
        return l <= r;
    case kOpLAnd:
        return l && r;
    case kOpLOr:
        return l || r;
    default:
        break;
    }
    assert(false);
    return 0;
}

void BinaryExp::Accept(Visitor *visitor) {
    if (visitor->Visit(this)) {
        left->Accept(visitor);
        right->Accept(visitor);
    }
}

json_t Array::to_json() const {
    json_t _json;
    for (auto x : dimensions) {
        if (x)
            _json.push_back(x->to_json());
        else
            _json.push_back(json_t(nullptr));
    }
    return _json;
}

void Array::VisitDimExps(Visitor *visitor) {
    for (auto x : dimensions)
        if (x)
            x->Accept(visitor);
}

json_t ArrayInitVal::to_json() const {
    json_t j = ArrayInitItem::to_json();
    j["children"] = {};
    for (auto child : children)
        j["children"].push_back(child->to_json());
    return j;
}

void ArrayInitVal::Accept(Visitor *visitor) {
    if (visitor->Visit(this)) {
        for (auto x : children) {
            if (x->kind == x->kExp)
                static_cast<Exp *>(x)->Accept(visitor);
            else if (x->kind == x->kList)
                static_cast<ArrayInitVal *>(x)->Accept(visitor);
            else
                assert(false);
        }
    }
}
