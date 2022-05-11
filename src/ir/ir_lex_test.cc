#include "ir/ir.hpp"
#include "token.h"
#include "ir.lex.h"
#include <gtest/gtest.h>
#include <string>

extern Token token;

int LexingToken(const char *buf) {
    YY_BUFFER_STATE s = yy_scan_string(strdup(buf));
    int r = yylex();
    yy_delete_buffer(s);
    return r;
}

TEST(IRLex, Comment) {
    ASSERT_EQ(LexingToken("; test%4$@#$!//*\n12"), Token::kInteger);
}

TEST(IRLex, Keyword) {
    ASSERT_EQ(LexingToken("declare"), Token::kDeclare);
    ASSERT_EQ(LexingToken("define"), Token::kDefine);
    ASSERT_EQ(LexingToken("global"), Token::kGlobal);
    ASSERT_EQ(LexingToken("constant"), Token::kConst);
}

TEST(IRLex, Inst) {
    ASSERT_EQ(LexingToken("call void @f()"), Token::kCall);
    ASSERT_EQ(LexingToken("ret"), Token::kRet);
    ASSERT_EQ(LexingToken("br"), Token::kBr);
}

TEST(IRLex, Type) {
    ASSERT_EQ(LexingToken("i1"), Token::kI1);
    ASSERT_EQ(LexingToken("i32"), Token::kI32);
    ASSERT_EQ(LexingToken("void"), Token::kVoid);
}

TEST(IRLex, Ident) {
    ASSERT_EQ(LexingToken("@test"), Token::kGlobalVar);
    ASSERT_EQ(std::get<std::string>(token.data), "test");
    ASSERT_EQ(LexingToken("@x1"), Token::kGlobalVar);
    ASSERT_EQ(std::get<std::string>(token.data), "x1");
}

TEST(IRLex, Integer) {
    ASSERT_EQ(LexingToken("123"), Token::kInteger);
    ASSERT_EQ(std::get<int>(token.data), 123);
    ASSERT_EQ(LexingToken("0x123"), Token::kInteger);
    ASSERT_EQ(std::get<int>(token.data), 0x123);
}

TEST(IRLex, Other) {
    ASSERT_EQ(LexingToken("*"), Token::kPtr);
    ASSERT_EQ(LexingToken("="), Token::kEq);
    ASSERT_EQ(LexingToken(","), Token::kComma);
    ASSERT_EQ(LexingToken("["), Token::kLbracket);
    ASSERT_EQ(LexingToken("]"), Token::kRbracket);
    ASSERT_EQ(LexingToken("x"), Token::kX);
    ASSERT_EQ(LexingToken("("), Token::kLParenthesis);
    ASSERT_EQ(LexingToken(")"), Token::kRParenthesis);
}

TEST(IRLex, IgnoreLLVMSyntax) {
    ASSERT_EQ(LexingToken("source_filename = \"001.c\"\n*"), Token::kPtr);
    ASSERT_EQ(LexingToken("target datalayout = "
                          "\"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:"
                          "128-n8:16:32:64-S128\"\n*"),
              Token::kPtr);
    ASSERT_EQ(
        LexingToken(
            "attributes #0 = { noinline nounwind uwtable "
            "\"frame-pointer\"=\"all\" \"min-legal-vector-width\"=\"0\" "
            "\"no-trapping-math\"=\"true\" "
            "\"stack-protector-buffer-size\"=\"8\" \"target-cpu\"=\"x86-64\" "
            "\"target-features\"=\"+cx8,+fxsr,+mmx,+sse,+sse2,+x87\" "
            "\"tune-cpu\"=\"generic\" }\n*"),
        Token::kPtr);
    ASSERT_EQ(LexingToken("!llvm.module.flags = !{!0, !1, !2}\n*"),
              Token::kPtr);
    ASSERT_EQ(LexingToken("!0 = !{i32 1, !\"wchar_size\", i32 4}\n*"),
              Token::kPtr);
    ASSERT_EQ(LexingToken("#0 {"), Token::kLBrace);
}