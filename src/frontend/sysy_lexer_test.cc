#include "ast/ast.hpp"
#include "sysy.lex.h"
#include "sysy.tab.h"
#include <gtest/gtest.h>

using namespace ast;

int LexingToken(const char *buf) {
    YY_BUFFER_STATE s = yy_scan_string(strdup(buf));
    int r = yylex();
    yy_delete_buffer(s);
    return r;
}

TEST(SysYLexerTest, Comment) {
    ASSERT_EQ(CONST,
              LexingToken("// this is #!$14/~`\';\"\nconst int a = 10;"));
    ASSERT_EQ(CONST, LexingToken("/* ** // int a; * */const int a = /**/ 10;"));
    ASSERT_EQ('*',
              LexingToken("/* ** // int a; */ */\nconst int a = /**/ 10;"));
}

TEST(SysYLexerTest, Operator) {
    ASSERT_EQ(LOR, LexingToken("||"));
    ASSERT_EQ('!', LexingToken("!"));
}

TEST(SysYLexerTest, Integer) {
    ASSERT_EQ(INTEGER, LexingToken("123"));
    ASSERT_EQ(123, yylval.integer);
    ASSERT_EQ(INTEGER, LexingToken("0"));
    ASSERT_NE(INTEGER, LexingToken("-123"));
    ASSERT_EQ(INTEGER, LexingToken("0x1b"));
    ASSERT_EQ(0x1b, yylval.integer);
    ASSERT_EQ(INTEGER, LexingToken("075"));
    ASSERT_EQ(075, yylval.integer);
}

TEST(SysYLexerTest, Identifier) {
    ASSERT_EQ(IDENT, LexingToken("_test"));
    ASSERT_EQ(0, strcmp("_test", yylval.ident)) << " " << yylval.ident;
    ASSERT_EQ(IDENT, LexingToken("test123"));
    ASSERT_EQ(0, strcmp("test123", yylval.ident)) << " " << yylval.ident;
    ASSERT_EQ(0, strcmp("test123", yylval.ident));
    ASSERT_NE(IDENT, LexingToken("0est"));
    ASSERT_EQ(INTEGER, LexingToken("0est"));
    ASSERT_EQ(IDENT, LexingToken("intval"));
    ASSERT_EQ(0, strcmp("intval", yylval.ident)) << " " << yylval.ident;
}

TEST(SysYLexerTest, Keyword) {
    ASSERT_EQ(INT, LexingToken("int"));
    ASSERT_EQ(CONST, LexingToken("const"));
    ASSERT_EQ(IF, LexingToken("if"));
    ASSERT_EQ(ELSE, LexingToken("else"));
    ASSERT_EQ(WHILE, LexingToken("while"));
    ASSERT_EQ(BREAK, LexingToken("break"));
    ASSERT_EQ(CONTINUE, LexingToken("continue"));
    ASSERT_EQ(RET, LexingToken("return"));
}
