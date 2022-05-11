#include "frontend.h"
#include "sysy.lex.h"
#include "sysy.tab.h"
#include "json.hpp"
#include <gtest/gtest.h>

using namespace ast;

extern CompUnit *comp_unit;

int TestParsing(const char *buf) {
    YY_BUFFER_STATE s = yy_scan_string(strdup(buf));
    int r = yyparse();
    yy_delete_buffer(s);
    return r;
}

void TestSyntax(const char *buf) { ASSERT_EQ(0, TestParsing(buf)); }

#define EQ_EXPECTED(expected)                                                  \
    ASSERT_EQ(comp_unit->to_json(), json_t(expected))                          \
        << comp_unit->to_json().dump();

TEST(SysYParserTest, ReduceReduceConflict) {
    // IDENT can be both Exp and VarDef
    TestParsing("void main() { a + b; }");
    EQ_EXPECTED(
        R"({"decls":null,"funcs":[{"block":{"op":"kOpBlock","stmts":[{"exp":{"left":{"name":"a","op":"kOpVar"},"op":"kOpAdd","right":{"name":"b","op":"kOpVar"}},"op":"kOpExpStmt"}]},"name":"main","op":"kOpFunc","ret":"void"}]})"_json);
    TestParsing("int a;");
    EQ_EXPECTED(
        R"({"decls":[{"init":null,"is_const":false,"name":"a","op":"kOpBDecl","type":"int"}],"funcs":null})"_json);
}

TEST(SysYParserTest, ConstDecl) {
    TestParsing("const int a = 12;");
    EQ_EXPECTED(
        R"({"decls":[{"init":{"op":"kOpConst","val":12},"is_const":true,"name":"a","op":"kOpBDecl","type":"int"}],"funcs":null})"_json);

    TestParsing("const int a = 10 * 2 + 2, bf = 11 - 1;");
    EQ_EXPECTED(
        R"({"decls":[{"init":{"left":{"left":{"op":"kOpConst","val":10},"op":"kOpMul","right":{"op":"kOpConst","val":2}},"op":"kOpAdd","right":{"op":"kOpConst","val":2}},"is_const":true,"name":"a","op":"kOpBDecl","type":"int"},{"init":{"left":{"op":"kOpConst","val":11},"op":"kOpSub","right":{"op":"kOpConst","val":1}},"is_const":true,"name":"bf","op":"kOpBDecl","type":"int"}],"funcs":null})"_json);

    TestParsing("const int a = 10 * 2 + 2; const int b = 10 + a;");
    EQ_EXPECTED(
        R"({"decls":[{"init":{"left":{"left":{"op":"kOpConst","val":10},"op":"kOpMul","right":{"op":"kOpConst","val":2}},"op":"kOpAdd","right":{"op":"kOpConst","val":2}},"is_const":true,"name":"a","op":"kOpBDecl","type":"int"},{"init":{"left":{"op":"kOpConst","val":10},"op":"kOpAdd","right":{"name":"a","op":"kOpVar"}},"is_const":true,"name":"b","op":"kOpBDecl","type":"int"}],"funcs":null})"_json);

    ASSERT_NE(0, TestParsing("const int a;"));

    TestParsing("const int array[10] = { 1, 2 * 3, 3 * 4};");
    EQ_EXPECTED(
        R"({"decls":[{"init":{"children":[{"op":"kOpConst","val":1},{"left":{"op":"kOpConst","val":2},"op":"kOpMul","right":{"op":"kOpConst","val":3}},{"left":{"op":"kOpConst","val":3},"op":"kOpMul","right":{"op":"kOpConst","val":4}}],"op":"kOpArrayInit"},"is_const":true,"name":"array","op":"kOpArrayDecl","type":{"count":[{"op":"kOpConst","val":10}],"element":"int"}}],"funcs":null})"_json);
}

TEST(SysYParserTest, VarDecl) {
    TestParsing("int a;");
    EQ_EXPECTED(
        R"( {"decls":[{"init":null,"is_const":false,"name":"a","op":"kOpBDecl","type":"int"}],"funcs":null})"_json);

    TestParsing("int a, b = 2 * 3;");
    EQ_EXPECTED(
        R"({"decls":[{"init":null,"is_const":false,"name":"a","op":"kOpBDecl","type":"int"},{"init":{"left":{"op":"kOpConst","val":2},"op":"kOpMul","right":{"op":"kOpConst","val":3}},"is_const":false,"name":"b","op":"kOpBDecl","type":"int"}],"funcs":null})"_json);

    TestParsing("int x; int a = 1; int b = a * 10 + x;");
    EQ_EXPECTED(
        R"({"decls":[{"init":null,"is_const":false,"name":"x","op":"kOpBDecl","type":"int"},{"init":{"op":"kOpConst","val":1},"is_const":false,"name":"a","op":"kOpBDecl","type":"int"},{"init":{"left":{"left":{"name":"a","op":"kOpVar"},"op":"kOpMul","right":{"op":"kOpConst","val":10}},"op":"kOpAdd","right":{"name":"x","op":"kOpVar"}},"is_const":false,"name":"b","op":"kOpBDecl","type":"int"}],"funcs":null})"_json);

    TestParsing("int a = 3;\nint b = 5;");
    EQ_EXPECTED(
        R"({"decls":[{"init":{"op":"kOpConst","val":3},"is_const":false,"name":"a","op":"kOpBDecl","type":"int"},{"init":{"op":"kOpConst","val":5},"is_const":false,"name":"b","op":"kOpBDecl","type":"int"}],"funcs":null})"_json);

    TestSyntax("int i = 0, sum = 1;");
    TestSyntax("int a, b0, _c;");
}

TEST(SysYParserTest, Func) {
    TestParsing("int sum(int a, int b, int c) { } void main() { }");
    EQ_EXPECTED(
        R"( {"decls":null,"funcs":[{"args":[{"init":null,"is_const":false,"name":"a","op":"kOpBDecl","type":"int"},{"init":null,"is_const":false,"name":"b","op":"kOpBDecl","type":"int"},{"init":null,"is_const":false,"name":"c","op":"kOpBDecl","type":"int"}],"block":{"op":"kOpBlock","stmts":[]},"name":"sum","op":"kOpFunc","ret":"int"},{"block":{"op":"kOpBlock","stmts":[]},"name":"main","op":"kOpFunc","ret":"void"}]})"_json);

    TestParsing("int main(){\nint a = 5;\nreturn a + b;\n}");
    EQ_EXPECTED(
        R"({"decls":null,"funcs":[{"block":{"op":"kOpBlock","stmts":[{"decls":[{"init":{"op":"kOpConst","val":5},"is_const":false,"name":"a","op":"kOpBDecl","type":"int"}],"op":"kOpDeclStmt"},{"op":"kOpReturn","val":{"left":{"name":"a","op":"kOpVar"},"op":"kOpAdd","right":{"name":"b","op":"kOpVar"}}}]},"name":"main","op":"kOpFunc","ret":"int"}]})"_json);
}

TEST(SysYParserTest, Array) {

    TestSyntax("int a[4][2] = {};");
    EQ_EXPECTED(
        R"({"decls":[{"init":{"children":null,"op":"kOpArrayInit"},"is_const":false,"name":"a","op":"kOpArrayDecl","type":{"count":[{"op":"kOpConst","val":4},{"op":"kOpConst","val":2}],"element":"int"}}],"funcs":null}
        )"_json);
    TestSyntax("int b[4][2] = {1, 2, 3, 4, 5, 6, 7, 8};");
    EQ_EXPECTED(
        R"({"decls":[{"init":{"children":[{"op":"kOpConst","val":1},{"op":"kOpConst","val":2},{"op":"kOpConst","val":3},{"op":"kOpConst","val":4},{"op":"kOpConst","val":5},{"op":"kOpConst","val":6},{"op":"kOpConst","val":7},{"op":"kOpConst","val":8}],"op":"kOpArrayInit"},"is_const":false,"name":"b","op":"kOpArrayDecl","type":{"count":[{"op":"kOpConst","val":4},{"op":"kOpConst","val":2}],"element":"int"}}],"funcs":null})"_json);
    TestSyntax("int c[4][2] = {{1, 2}, {3, 4}, {5, 6}, {7, 8}};");
    EQ_EXPECTED(
        R"( {"decls":[{"init":{"children":[{"children":[{"op":"kOpConst","val":1},{"op":"kOpConst","val":2}],"op":"kOpArrayInit"},{"children":[{"op":"kOpConst","val":3},{"op":"kOpConst","val":4}],"op":"kOpArrayInit"},{"children":[{"op":"kOpConst","val":5},{"op":"kOpConst","val":6}],"op":"kOpArrayInit"},{"children":[{"op":"kOpConst","val":7},{"op":"kOpConst","val":8}],"op":"kOpArrayInit"}],"op":"kOpArrayInit"},"is_const":false,"name":"c","op":"kOpArrayDecl","type":{"count":[{"op":"kOpConst","val":4},{"op":"kOpConst","val":2}],"element":"int"}}],"funcs":null})"_json);
    // not allowed, it can be checked later
    TestSyntax("int d[4][2] = {1, 2, {3}, {5}, 7 , 8};");
    EQ_EXPECTED(
        R"({"decls":[{"init":{"children":[{"op":"kOpConst","val":1},{"op":"kOpConst","val":2},{"children":[{"op":"kOpConst","val":3}],"op":"kOpArrayInit"},{"children":[{"op":"kOpConst","val":5}],"op":"kOpArrayInit"},{"op":"kOpConst","val":7},{"op":"kOpConst","val":8}],"op":"kOpArrayInit"},"is_const":false,"name":"d","op":"kOpArrayDecl","type":{"count":[{"op":"kOpConst","val":4},{"op":"kOpConst","val":2}],"element":"int"}}],"funcs":null})"_json);
    TestSyntax("int d[4][2] = {{1, 2}, {3}, {5}, 7 , 8};");
    EQ_EXPECTED(
        R"({"decls":[{"init":{"children":[{"children":[{"op":"kOpConst","val":1},{"op":"kOpConst","val":2}],"op":"kOpArrayInit"},{"children":[{"op":"kOpConst","val":3}],"op":"kOpArrayInit"},{"children":[{"op":"kOpConst","val":5}],"op":"kOpArrayInit"},{"op":"kOpConst","val":7},{"op":"kOpConst","val":8}],"op":"kOpArrayInit"},"is_const":false,"name":"d","op":"kOpArrayDecl","type":{"count":[{"op":"kOpConst","val":4},{"op":"kOpConst","val":2}],"element":"int"}}],"funcs":null})"_json);
}

TEST(SysYParserTest, IfIfElse) {
    TestSyntax("void f() { if(a == 5) if (b == 10) a = 25; else a = a + 15; }");
    EQ_EXPECTED(
        R"({"decls":null,"funcs":[{"block":{"op":"kOpBlock","stmts":[{"cond":{"left":{"name":"a","op":"kOpVar"},"op":"kOpEq","right":{"op":"kOpConst","val":5}},"fstmt":null,"op":"kOpIf","tstmt":{"cond":{"left":{"name":"b","op":"kOpVar"},"op":"kOpEq","right":{"op":"kOpConst","val":10}},"fstmt":{"lval":{"name":"a","op":"kOpVar"},"op":"kOpAssign","rval":{"left":{"name":"a","op":"kOpVar"},"op":"kOpAdd","right":{"op":"kOpConst","val":15}}},"op":"kOpIf","tstmt":{"lval":{"name":"a","op":"kOpVar"},"op":"kOpAssign","rval":{"op":"kOpConst","val":25}}}}]},"name":"f","op":"kOpFunc","ret":"void"}]})"_json);
}

TEST(SysYParserTest, Syntax) {
    TestSyntax("int main(){int a;a = 10;return a % 3;}");

    TestSyntax("int a = putint(ifElseIf());");
    TestSyntax("void f() { if (1) { print(); } else exit(); }");

    TestSyntax("int x = (a == 6 || b == 0xb);");

    TestSyntax("int arr[N + 2 * 4 - 99 / 99] = {1, 2, 33, 4, 5, 6};");

    TestSyntax("void f() { min=i; //\n}");
}