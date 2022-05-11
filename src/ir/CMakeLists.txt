# ir 
add_library(ir OBJECT  ir.cc)

# ir parser lib
find_package(FLEX)

FLEX_TARGET(IRLexer ir.l
  ${CMAKE_CURRENT_SOURCE_DIR}/ir.lex.cc
  DEFINES_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ir.lex.h
)

set(IR_PARSER_SRC
  "parser.cc"
  ${FLEX_IRLexer_OUTPUTS}
)

add_library(ir_parser ${IR_PARSER_SRC})
target_link_libraries(ir_parser ir)

# irparser tool
add_executable(irparser irparser.cc)
target_link_libraries(irparser ir_parser)

## test objects
include(GoogleTest)

# test ir lexer
add_executable(ir_lex_test
  ir_lex_test.cc
  ${FLEX_IRLexer_OUTPUTS}
)
target_link_libraries(ir_lex_test gtest_main ir)
gtest_discover_tests(ir_lex_test)

# test ir parser
add_executable(ir_parser_test
  parser_test.cc
  ${FLEX_IRLexer_OUTPUTS}
)
target_link_libraries(ir_parser_test gtest_main ir_parser)
gtest_discover_tests(ir_parser_test)
