#include "error.h"
#include "frontend.h"
#include "sysy.lex.h"
#include "sysy.tab.h"
#include "parser.h"
#include <gtest/gtest.h>

using namespace ast;

extern CompUnit *SemanticCheck(CompUnit *comp_unit);

CompUnit *TestParsing(const char *buf) { return Parse(buf, strlen(buf)); }

TEST(SemanticCheck, UseBeforeDefine) {
    TestParsing("int a = b;");
    ASSERT_THROW(SemanticCheck(comp_unit), SymbolNotDefine);
    TestParsing("int b; int a = b;");
    SemanticCheck(comp_unit);
    TestParsing("int a = a;");
    ASSERT_THROW(SemanticCheck(comp_unit), SymbolNotDefine);
    TestParsing("int a[][2] = {a};");
    ASSERT_THROW(SemanticCheck(comp_unit), SymbolNotDefine);
    TestParsing("void main() { f(1); }");
    ASSERT_THROW(SemanticCheck(comp_unit), SymbolNotDefine);
}

TEST(SemanticCheck, BreakContinueNotInWhile) {
    TestParsing("void f() { break; }");
    ASSERT_THROW(SemanticCheck(comp_unit), BreakContiException);
    TestParsing("void f() { continue; }");
    ASSERT_THROW(SemanticCheck(comp_unit), BreakContiException);
    TestParsing("void f() {  while(1) break; }");
    SemanticCheck(comp_unit);
    TestParsing("void f() {  while(1) continue; }");
    SemanticCheck(comp_unit);
}

#define EQ_EXPECTED(expected)                                                  \
    ASSERT_EQ(comp_unit->to_json(), json_t(expected)) << comp_unit->to_json();

TEST(SemanticCheck, Init) {
    TestParsing("int a = 10, b = a + 2;");
    SemanticCheck(comp_unit);
    EQ_EXPECTED(
        R"({"decls":[{"init":{"op":"kOpConst","val":10},"is_const":false,"name":"a","op":"kOpBDecl","type":"int"},{"init":{"op":"kOpConst","val":12},"is_const":false,"name":"b","op":"kOpBDecl","type":"int"}],"funcs":null}
        )"_json);
    TestParsing("int a = 10, b[][3] = {{a + 2}, {11, 13}, 12};");
    ASSERT_NO_THROW(SemanticCheck(comp_unit));
    EQ_EXPECTED(
        R"({"decls":[{"init":{"op":"kOpConst","val":10},"is_const":false,"name":"a","op":"kOpBDecl","type":"int"},{"init":{"children":[{"children":[{"op":"kOpConst","val":12}],"op":"kOpArrayInit"},{"children":[{"op":"kOpConst","val":11},{"op":"kOpConst","val":13}],"op":"kOpArrayInit"},{"op":"kOpConst","val":12}],"op":"kOpArrayInit"},"init_val_records":[{"pos":0,"vals":[{"op":"kOpConst","val":12}]},{"pos":3,"vals":[{"op":"kOpConst","val":11},{"op":"kOpConst","val":13}]},{"pos":6,"vals":[{"op":"kOpConst","val":12}]}],"is_const":false,"name":"b","op":"kOpArrayDecl","type":{"count":[3,3],"element":"int"}}],"funcs":null}
        )"_json);
    TestParsing("void main() { int b[] = {2, 3}; int c[][2] = {0};}");
    ASSERT_NO_THROW(SemanticCheck(comp_unit));
    EQ_EXPECTED(
        R"({"decls":null,"funcs":[{"block":{"op":"kOpBlock","stmts":[{"decls":[{"init":{"children":[{"op":"kOpConst","val":2},{"op":"kOpConst","val":3}],"op":"kOpArrayInit"},"init_val_records":[{"pos":0,"vals":[{"op":"kOpConst","val":2},{"op":"kOpConst","val":3}]}],"is_const":false,"name":"b","op":"kOpArrayDecl","type":{"count":[2],"element":"int"}}],"op":"kOpDeclStmt"},{"decls":[{"init":{"children":[{"op":"kOpConst","val":0}],"op":"kOpArrayInit"},"init_val_records":[{"pos":0,"vals":[{"op":"kOpConst","val":0}]}],"is_const":false,"name":"c","op":"kOpArrayDecl","type":{"count":[1,2],"element":"int"}}],"op":"kOpDeclStmt"}]},"name":"main","op":"kOpFunc","ret":"void"}]}
        )"_json);
    TestParsing("int d[4][2] = {1, 2, {3}, {5}, 7 , 8};");
    ASSERT_NO_THROW(SemanticCheck(comp_unit));
    EQ_EXPECTED(
        R"({"decls":[{"init":{"children":[{"op":"kOpConst","val":1},{"op":"kOpConst","val":2},{"children":[{"op":"kOpConst","val":3}],"op":"kOpArrayInit"},{"children":[{"op":"kOpConst","val":5}],"op":"kOpArrayInit"},{"op":"kOpConst","val":7},{"op":"kOpConst","val":8}],"op":"kOpArrayInit"},"init_val_records":[{"pos":0,"vals":[{"op":"kOpConst","val":1},{"op":"kOpConst","val":2}]},{"pos":2,"vals":[{"op":"kOpConst","val":3}]},{"pos":4,"vals":[{"op":"kOpConst","val":5}]},{"pos":6,"vals":[{"op":"kOpConst","val":7},{"op":"kOpConst","val":8}]}],"is_const":false,"name":"d","op":"kOpArrayDecl","type":{"count":[4,2],"element":"int"}}],"funcs":null}
        )"_json);
    TestParsing("int a = 7, b = 15, x[1] = {1}, y[1] = {1};");
    ASSERT_NO_THROW(SemanticCheck(comp_unit));
    EQ_EXPECTED(
        R"({"decls":[{"init":{"op":"kOpConst","val":7},"is_const":false,"name":"a","op":"kOpBDecl","type":"int"},{"init":{"op":"kOpConst","val":15},"is_const":false,"name":"b","op":"kOpBDecl","type":"int"},{"init":{"children":[{"op":"kOpConst","val":1}],"op":"kOpArrayInit"},"init_val_records":[{"pos":0,"vals":[{"op":"kOpConst","val":1}]}],"is_const":false,"name":"x","op":"kOpArrayDecl","type":{"count":[1],"element":"int"}},{"init":{"children":[{"op":"kOpConst","val":1}],"op":"kOpArrayInit"},"init_val_records":[{"pos":0,"vals":[{"op":"kOpConst","val":1}]}],"is_const":false,"name":"y","op":"kOpArrayDecl","type":{"count":[1],"element":"int"}}],"funcs":null}
        )"_json);

    //
    // TestParsing("int f(int arr[], int count) { return 0; }");
    // ASSERT_NO_THROW(SemanticCheck(comp_unit));
    // EQ_EXPECTED(R"( {}
    //     )"_json);
}