#include "ast/ast.hpp"
#include <gtest/gtest.h>

using namespace ast;

TEST(AST, All) {
    CompUnit comp;
    auto bdecl_a = new BDecl(g_type_system.TInt(), "a", new ConstExp(0));
    comp.decls.insert(bdecl_a->name, bdecl_a);
    auto decl_f =
        new FuncDecl("f", g_type_system.TVoid(), {g_type_system.TInt()});
    comp.decls.insert(decl_f->name, decl_f);

    auto block = new BlockStmt({});
    auto func = new Func("func", g_type_system.TInt(), {}, block);
    block->SetFunc(func);
    func->args.push_back(new BDecl(g_type_system.TInt(), "a"));
    block->stmts.push_back(new ReturnStmt(
        new BinaryExp(AstNode::kOpAdd, new VarExp("a"), new ConstExp(1))));
    comp.funcs.insert(func->name, func);
}