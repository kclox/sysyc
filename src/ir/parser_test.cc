#include "parser.h"
#include <gtest/gtest.h>
#include <memory>

class ParseTester {
  protected:
    YY_BUFFER_STATE s;

  public:
    ir::Module m;
    ir::Func func;
    Lexer lexer;
    Parser parser = Parser(lexer, m);

    ParseTester(char *buf) {
        s = yy_scan_string(strdup(buf));
        parser.PrepareForTest();
    }

    virtual ~ParseTester() {
        yy_delete_buffer(s);
        yylex_destroy();
    }
};

class ParseInstTester : public ParseTester {
  public:
    std::shared_ptr<ir::Value> result;

    ParseInstTester(char *buf) : ParseTester(buf) {
        if (parser.Current().IsVar()) {
            result = parser.ParseValue(ir::t_undef, &func);
            parser.ConsumeToken(Token::kEq);
        }
    }
};

TEST(IRParser, Type) {}

TEST(IRParser, Getelementptr) {
    char buf[] = "%1 = getelementptr [3 x i32], [3 x i32]* "
                 "%__main._0_8_7.c, i32 0, i32 0";
    auto t = ParseInstTester(buf);
    auto inst = t.parser.ParseGetelementptr(t.result, &t.func);
    ASSERT_EQ(std::string(buf), inst->str());
    ASSERT_EQ(ir::t_i32->kind, ir::Type::kI32);
}
