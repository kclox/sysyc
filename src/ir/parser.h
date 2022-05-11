#ifndef __parser_h__
#define __parser_h__

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <map>
#include <set>
#include <sstream>
#include <variant>
#include <regex>
#include "token.h"
#include "location.h"
#include "ir.lex.h"
#include "ir/ir.hpp"

extern Token token;
extern Location g_ir_l_location;
extern bool g_debug_enabed;

template <typename... Args> void Debug(const char *fmt, Args... args) {
    if (g_debug_enabed)
        printf(fmt, args...);
}

class Lexer {
  private:
    FILE *fin;
    Token current;

  public:
    Location &loc;

    Lexer() : loc(g_ir_l_location) { fin = nullptr; }

    Lexer(FILE *fin) : fin(fin), loc(g_ir_l_location) { yyin = fin; }

    Token NextToken() {
        current.op = yylex();
        current.data = token.data;
        Debug("token.op = %s\n", current.str().c_str());
        return current;
    }

    bool HasNext() { return !current.IsEnd(); }
};

class Parser {
  private:
    Lexer &lexer;
    ir::Module &m;
    Token current;

  public:
    Parser(Lexer &lexer, ir::Module &m) : lexer(lexer), m(m) {}

    void Parse() {
        Debug("+Parse\n");
        current = lexer.NextToken();
        do {
            ParseTopLevel();
        } while (lexer.HasNext());
        Debug("-Parse\n");
    }

    void PrepareForTest() { current = lexer.NextToken(); }

    void ParseTopLevel() {
        Debug("+ParseTopLevel\n");
        switch (current.op) {
        case Token::kGlobalVar: {
            // global define
            Debug("define global var %s\n",
                  std::get<std::string>(current.data).c_str());
            m.defs.push_back(ParseDefine());
        } break;
        case Token::kDeclare: {
            // function declare
            Debug("function declare\n");
            m.InsertFuncDecl(ParseDecl());
        } break;
        case Token::kDefine: {
            // function define
            Debug("function define\n");
            m.InsertFunc(ParseFunc());
        } break;
        case Token::kEnd:
            break;
        default:
            assert(false ||
                   SyntaxError("invalid token: %s\n", token.str().c_str()));
        }
        Debug("-ParseTopLevel\n");
    }

    std::unique_ptr<ir::Define> ParseDefine() {
        auto def = std::make_unique<ir::Define>();
        std::string name = std::get<std::string>(current.data);
        def->is_const = false;
        current = lexer.NextToken();
        ConsumeToken(Token::kEq);
        if (current.op == Token::kConst) {
            def->is_const = true;
            current = lexer.NextToken();
        } else if (current.op == Token::kGlobal) {
            current = lexer.NextToken();
        }
        def->init = ParseInit();
        def->var = m.CreateGlobalVar(def->init->type(), name);
        return def;
    }

    ir::FuncDecl *ParseDecl() {
        current = lexer.NextToken();
        auto decl = new ir::FuncDecl();
        decl->ft = std::make_shared<ir::FuncT>();
        decl->ft->ret = ParseType();
        assert(current.op == Token::kGlobalVar);
        decl->name = std::get<std::string>(current.data);
        current = lexer.NextToken();
        ConsumeToken(Token::kLParenthesis);
        while (current.op != Token::kRParenthesis) {
            decl->ft->args.push_back(ParseType());
            if (current.op == Token::kComma)
                current = lexer.NextToken();
        }
        current = lexer.NextToken();
        return decl;
    }

    ir::Func *ParseFunc() {
        auto func = new ir::Func();
        auto ft = std::make_shared<ir::FuncT>();
        func->ft = ft;
        // define ret @f(...) { }
        current = lexer.NextToken();
        ft->ret = ParseType();
        assert(current.op == Token::kGlobalVar);
        func->name = std::get<std::string>(current.data);
        current = lexer.NextToken();
        // (...)
        ConsumeToken(Token::kLParenthesis);
        while (current.op != Token::kRParenthesis) {
            auto ty = ParseType();
            ft->args.push_back(ty);
            assert(current.IsVar() && current.op != Token::kGlobalVar);
            auto arg = ParseVar(ty, func);
            func->args.push_back(
                std::dynamic_pointer_cast<ir::LocalValue>(arg));
            if (current.op == Token::kComma) {
                current = lexer.NextToken();
            }
        }
        current = lexer.NextToken();
        // { ... }
        ConsumeToken(Token::kLBrace);
        auto bb = std::make_unique<ir::BB>();
        bb->func = func;
        bool first_bb = true;
        while (current.op != Token::kRBrace) {
            // find next label
            if (current.IsLabel()) {
                if (!first_bb) {
                    // push back old bb
                    func->bblocks.push_back(std::move(bb));
                    // create new bb
                    bb = std::make_unique<ir::BB>();
                    bb->func = func;
                }
                auto label = ParseLabel(func);
                func->label_map[label.get()] = bb.get();
                bb->label = label;
            }
            // parse instruction
            if (current.IsInst()) {
                bb->insts.push_back(ParseInst(bb.get(), nullptr));
            } else if (current.IsVar()) {
                auto res = ParseVar(ir::t_undef, func);
                ConsumeToken(Token::kEq);
                bb->insts.push_back(ParseInst(bb.get(), res));
            } else if (current.op != Token::kRBrace) {
                assert(false || SyntaxError("invalid token: %s\n",
                                            current.str().c_str()));
            }
            first_bb = false;
        }
        func->bblocks.push_back(std::move(bb));
        current = lexer.NextToken();
        return func;
    }

    std::shared_ptr<ir::Type> ParseType() {
        Debug("+ParseType\n");
        std::shared_ptr<ir::Type> res;
        switch (current.op) {
        case Token::kI32:
            res = ir::t_i32;
            break;
        case Token::kI1:
            res = ir::t_i1;
            break;
        case Token::kVoid:
            res = ir::t_void;
            break;
        case Token::kLabelT:
            res = ir::t_label;
            break;
        case Token::kLbracket: {
            current = lexer.NextToken();
            assert(current.op == Token::kInteger);
            int count = std::get<int>(current.data);
            current = lexer.NextToken();
            ConsumeToken(Token::kX);
            auto elem_ty = ParseType();
            assert(current.op == Token::kRbracket);
            res = std::make_shared<ir::ArrayT>(elem_ty, count);
        } break;
        default:
            throw std::runtime_error("invalid type");
        }
        current = lexer.NextToken();
        while (current.op == Token::kPtr) {
            res = std::make_shared<ir::PtrT>(res);
            current = lexer.NextToken();
        }
        Debug("-ParseType\n");
        return res;
    }

    std::shared_ptr<ir::Value> ParseValue(std::shared_ptr<ir::Type> ty,
                                          ir::Func *func = nullptr) {
        std::shared_ptr<ir::Value> res;
        if (current.IsVar())
            res = ParseVar(ty, func);
        else if (current.op == Token::kInteger) {
            res = ParseImm(ty);
        } else {
            assert(false);
        }
        return res;
    }

    std::shared_ptr<ir::ImmValue> ParseImm(std::shared_ptr<ir::Type> ty) {
        std::shared_ptr<ir::ImmValue> res;
        if (ty->kind == ir::Type::kI32) {
            res = m.CreateImm(std::get<int>(current.data));
        } else {
            res =
                std::make_shared<ir::ImmValue>(ty, std::get<int>(current.data));
        }
        current = lexer.NextToken();
        return res;
    }

    std::shared_ptr<ir::Value> ParseVar(std::shared_ptr<ir::Type> ty,
                                        ir::Func *func = nullptr) {
        std::shared_ptr<ir::Value> res;
        switch (current.op) {
        case Token::kGlobalVar: {
            res = m.CreateGlobalVar(ty, std::get<std::string>(current.data));
        } break;
        case Token::kLocalVar: {
            assert(func != nullptr ||
                   SyntaxError("can not parse local var without func\n"));
            res = func->CreateLocalVar(ty, std::get<std::string>(current.data));
        } break;
        case Token::kTmpVar: {
            assert(func != nullptr);
            int id = std::get<int>(current.data);
            res = CreateTmpVar(func, ty, id);
        } break;
        default:
            assert(false || SyntaxError("parse var fail: %d\n", current.op));
        }
        current = lexer.NextToken();
        return res;
    }

    std::shared_ptr<ir::Value> ParseTypedValue(ir::Func *func = nullptr) {
        auto ty = ParseType();
        auto value = ParseValue(ty, func);
        return value;
    }

    std::shared_ptr<ir::LocalValue> ParseLabel(ir::Func *func) {
        std::shared_ptr<ir::LocalValue> label;
        switch (current.op) {
        case Token::kNamedLabel: {
            label = func->CreateLocalVar(ir::t_label,
                                         std::get<std::string>(current.data));
        } break;
        case Token::kTmpLabel: {
            label =
                CreateTmpVar(func, ir::t_label, std::get<int>(current.data));
        } break;
        }
        current = lexer.NextToken();
        return label;
    }

    std::unique_ptr<ir::InitVal> ParseInit() {
        // BUG
        if (current.IsBasicType()) {
            auto ty = ParseType();
            // now, only support integer init
            assert(current.op == Token::kInteger);
            auto init = std::make_unique<ir::BasicInit>();
            init->val = ParseImm(ty);
            return init;
        }
        // for array or array*
        if (current.op == Token::kLbracket) {
            auto ty = ParseType();
            if (ty->kind == ir::Type::kArray) {
                auto init = std::make_unique<ir::ArrayInit>();
                init->ty = std::dynamic_pointer_cast<ir::ArrayT>(ty);
                // [ ty ] [ vals ]
                if (current.op == Token::kLbracket) {
                    current = lexer.NextToken();
                    while (current.op != Token::kRbracket) {
                        init->vals.push_back(ParseInit());
                        if (current.op == Token::kComma)
                            current = lexer.NextToken();
                    }
                    current = lexer.NextToken();
                    return init;
                } else {
                    ConsumeToken(Token::kZeroinit);
                    return std::make_unique<ir::Zeroinitializer>(ty);
                }
            } else {
                auto init = std::make_unique<ir::BasicInit>();
                init->val = ParseImm(ty);
                return init;
            }
        }
        assert(false || SyntaxError("init not supported\n"));
    }

    std::unique_ptr<ir::Instr> ParseInst(ir::BB *bb,
                                         std::shared_ptr<ir::Value> result) {
        switch (current.op) {
        case Token::kCall:
            return ParseCall(result, bb->func);
        case Token::kRet:
            return ParseRet(bb->func);
        case Token::kBr:
            return ParseRr(bb->func);
        case Token::kIcmp:
            return ParseIcmp(result, bb->func);
        case Token::kLoad:
            return ParseLoad(result, bb->func);
        case Token::kStore:
            return ParseStore(bb->func);
        case Token::kAlloca:
            return ParseAlloca(result, bb->func);
        case Token::kGetelementptr:
            return ParseGetelementptr(result, bb->func);
        case Token::kPhi:
            return ParsePhi(result, bb->func);
        case Token::kZext:
            return ParseZext(result, bb->func);
        case Token::kBitcast:
            return ParseBitcast(result, bb->func);
        default:
            assert(current.IsBinaryAlu());
            return ParseBinaryAlu(result, bb->func);
        }
    }

    std::unique_ptr<ir::Instr> ParseCall(std::shared_ptr<ir::Value> result,
                                         ir::Func *func) {
        Debug("+ParseCall\n");
        current = lexer.NextToken();
        auto call = std::make_unique<ir::Call>();
        call->result = result;
        auto ft = std::make_shared<ir::FuncT>();
        ft->ret = ParseType();
        if (result && ft->ret->kind != ir::Type::kVoid)
            result->ty = ft->ret;
        assert(current.op == Token::kGlobalVar);
        call->func = std::dynamic_pointer_cast<ir::GlobalVar>(ParseVar(ft));
        // (...)
        ConsumeToken(Token::kLParenthesis);
        while (current.op != Token::kRParenthesis) {
            auto ty = ParseType();
            ft->args.push_back(ty);
            auto arg = ParseValue(ty, func);
            call->args.push_back(arg);
            if (current.op == Token::kComma) {
                current = lexer.NextToken();
            }
        }
        current = lexer.NextToken();
        Debug("-ParseCall\n");
        return call;
    }

    std::unique_ptr<ir::Instr> ParseRet(ir::Func *func) {
        Debug("+ParseRet\n");
        auto ret = std::make_unique<ir::Ret>();
        current = lexer.NextToken();
        ret->ty = ParseType();
        if (ret->ty->kind != ir::Type::kVoid)
            ret->retval = ParseValue(ret->ty, func);
        Debug("-ParseRet\n");
        return ret;
    }

    std::unique_ptr<ir::Instr> ParseRr(ir::Func *func) {
        Debug("+ParseBr\n");
        auto res = std::make_unique<ir::Br>();
        current = lexer.NextToken();
        auto ty1 = ParseType();
        // br label <label>
        if (ty1->kind == ir::Type::kLabel) {
            res->l1 = std::dynamic_pointer_cast<ir::LocalValue>(
                ParseValue(ty1, func));
        } else {
            assert(ty1->kind == ir::Type::kI1);
            res->cond = ParseValue(ty1, func);
            ConsumeToken(Token::kComma);
            res->l1 = std::dynamic_pointer_cast<ir::LocalValue>(
                ParseTypedValue(func));
            ConsumeToken(Token::kComma);
            res->l2 = std::dynamic_pointer_cast<ir::LocalValue>(
                ParseTypedValue(func));
        }
        Debug("-ParseBr\n");
        return res;
    }

    std::unique_ptr<ir::Instr> ParseIcmp(std::shared_ptr<ir::Value> result,
                                         ir::Func *func) {
        auto icmp = std::make_unique<ir::Icmp>();
        icmp->result = result;
        result->ty = ir::t_i1;
        current = lexer.NextToken();
        icmp->cond = ParseCond();
        icmp->ty = ParseType();
        icmp->l = ParseValue(icmp->ty, func);
        ConsumeToken(Token::kComma);
        icmp->r = ParseValue(icmp->ty, func);
        return icmp;
    }

    int ParseCond() {
        static std::map<int, int> m{
            {Token::kCondEq, ir::Icmp::kEq},
            {Token::kCondNe, ir::Icmp::kNe},
            {Token::kCondUgt, ir::Icmp::kUgt},
            {Token::kCondUge, ir::Icmp::kUge},
            {Token::kCondUle, ir::Icmp::kUle},
            {Token::kCondSgt, ir::Icmp::kSgt},
            {Token::kCondSge, ir::Icmp::kSge},
            {Token::kCondSlt, ir::Icmp::kSlt},
            {Token::kCondSle, ir::Icmp::kSle},
        };
        assert(current.IsCond());
        int cond = m[current.op];
        current = lexer.NextToken();
        return cond;
    }

    std::unique_ptr<ir::Instr> ParseBinaryAlu(std::shared_ptr<ir::Value> result,
                                              ir::Func *func) {
        int op = ParseBinaryAluOp();
        auto alu = std::make_unique<ir::BinaryAlu>(op);
        alu->result = result;
        alu->ty = ParseType();
        result->ty = alu->ty;
        alu->l = ParseValue(alu->ty, func);
        ConsumeToken(Token::kComma);
        alu->r = ParseValue(alu->ty, func);
        return alu;
    }

    int ParseBinaryAluOp() {
        static std::map<int, int> m{
            {Token::kAdd, ir::Instr::kOpAdd},
            {Token::kSub, ir::Instr::kOpSub},
            {Token::kMul, ir::Instr::kOpMul},
            {Token::kSdiv, ir::Instr::kOpSdiv},
            {Token::kSrem, ir::Instr::kOpSrem},
            {Token::kAnd, ir::Instr::kOpAnd},
            {Token::kOr, ir::Instr::kOpOr},
        };
        assert(current.IsBinaryAlu());
        int op = m[current.op];
        current = lexer.NextToken();
        return op;
    }

    std::unique_ptr<ir::Instr> ParseLoad(std::shared_ptr<ir::Value> result,
                                         ir::Func *func) {
        current = lexer.NextToken();
        auto load = std::make_unique<ir::Load>();
        load->result = result;
        load->ty = ParseType();
        result->ty = load->ty;
        ConsumeToken(Token::kComma);
        load->ptr = ParseTypedValue(func);
        if (current.op == Token::kComma) {
            current = lexer.NextToken();
            ConsumeToken(Token::kAlign);
            load->alignment = std::get<int>(current.data);
            current = lexer.NextToken();
        }
        return load;
    }

    std::unique_ptr<ir::Instr> ParseStore(ir::Func *func) {
        current = lexer.NextToken();
        auto store = std::make_unique<ir::Store>();
        store->val = ParseTypedValue(func);
        ConsumeToken(Token::kComma);
        store->ptr = ParseTypedValue(func);
        if (current.op == Token::kComma) {
            current = lexer.NextToken();
            ConsumeToken(Token::kAlign);
            store->alignment = std::get<int>(current.data);
            current = lexer.NextToken();
        }
        return store;
    }

    std::unique_ptr<ir::Instr> ParseAlloca(std::shared_ptr<ir::Value> result,
                                           ir::Func *func) {
        current = lexer.NextToken();
        auto alloca = std::make_unique<ir::Alloca>();
        alloca->result = result;
        alloca->ty = ParseType();
        result->ty = std::make_shared<ir::PtrT>(alloca->ty);
        if (current.op == Token::kComma) {
            current = lexer.NextToken();
            ConsumeToken(Token::kAlign);
            assert(current.op == Token::kInteger);
            alloca->alignment = std::get<int>(current.data);
            current = lexer.NextToken();
        }
        return alloca;
    }

    std::unique_ptr<ir::Instr>
    ParseGetelementptr(std::shared_ptr<ir::Value> result, ir::Func *func) {
        current = lexer.NextToken();
        auto inst = std::make_unique<ir::Getelementptr>();
        inst->result = result;
        inst->ty = ParseType();
        ConsumeToken(Token::kComma);
        inst->ptr = ParseTypedValue(func);
        while (current.op == Token::kComma) {
            current = lexer.NextToken();
            inst->indices.push_back(ParseTypedValue(func));
        }
        auto p = inst->ptr->ty;
        for (auto idx : inst->indices) {
            if (p->kind == ir::Type::kPtr) {
                p = std::dynamic_pointer_cast<ir::PtrT>(p)->p;
            } else if (p->kind = ir::Type::kArray) {
                p = std::dynamic_pointer_cast<ir::ArrayT>(p)->element;
            } else {
                assert(false);
            }
        }
        result->ty = std::make_shared<ir::PtrT>(p);
        return inst;
    }

    std::unique_ptr<ir::Instr> ParsePhi(std::shared_ptr<ir::Value> result,
                                        ir::Func *func) {
        current = lexer.NextToken();
        auto phi = std::make_unique<ir::Phi>();
        phi->result = result;
        phi->ty = ParseType();
        result->ty = phi->ty;
        while (current.op == Token::kLbracket) {
            current = lexer.NextToken();
            ir::Phi::PhiVal v;
            v.val = ParseValue(phi->ty, func);
            ConsumeToken(Token::kComma);
            v.label = std::dynamic_pointer_cast<ir::LocalValue>(
                ParseValue(ir::t_label, func));
            ConsumeToken(Token::kRbracket);
            phi->vals.push_back(v);
            if (current.op == Token::kComma)
                current = lexer.NextToken();
        }
        return phi;
    }

    std::unique_ptr<ir::Instr> ParseZext(std::shared_ptr<ir::Value> result,
                                         ir::Func *func) {
        current = lexer.NextToken();
        auto zext = std::make_unique<ir::Zext>();
        zext->result = result;
        zext->val = ParseTypedValue(func);
        ConsumeToken(Token::kTo);
        zext->ty_ext = ParseType();
        result->ty = zext->ty_ext;
        return zext;
    }

    std::unique_ptr<ir::Instr> ParseBitcast(std::shared_ptr<ir::Value> result,
                                            ir::Func *func) {
        current = lexer.NextToken();
        auto bitcast = std::make_unique<ir::Bitcast>();
        bitcast->result = result;
        bitcast->val = ParseTypedValue(func);
        ConsumeToken(Token::kTo);
        bitcast->ty_cast = ParseType();
        result->ty = bitcast->ty_cast;
        return bitcast;
    }

    inline void ConsumeToken(int op, bool next = true) {
        assert(current.op == op ||
               SyntaxError("token %s is expected not %s\n",
                           Token(op).str().c_str(), current.str().c_str()));
        if (next)
            current = lexer.NextToken();
    }

    template <typename... Args>
    bool SyntaxError(const char *fmt, Args... args) {
        char buf[0x1000] = "[Syntax Error] %s ";
        assert(strlen(fmt) <= 0x1000 - 18);
        strcat(buf, fmt);
        printf(buf, ((std::string)lexer.loc).c_str(), args...);
        return false;
    }

    const Token &Current() { return current; }

  private:
    std::map<int, std::shared_ptr<ir::TmpVar>> tmp_var_map;

    std::shared_ptr<ir::TmpVar>
    CreateTmpVar(ir::Func *func, std::shared_ptr<ir::Type> ty, int id = 0) {
        auto it = tmp_var_map.find(id);
        if (it != tmp_var_map.end())
            return it->second;
        auto tvar = func->CreateTmpVar(ty);
        tvar->id = id;
        tmp_var_map[id] = tvar;
        return tvar;
    }
};

#endif