find_package(BISON)
find_package(FLEX)

BISON_TARGET(SysYParser sysy.yy
  ${CMAKE_CURRENT_SOURCE_DIR}/sysy.tab.cc
  DEFINES_FILE ${CMAKE_CURRENT_SOURCE_DIR}/sysy.tab.h
)

FLEX_TARGET(SysYLexer sysy.l
  ${CMAKE_CURRENT_SOURCE_DIR}/sysy.lex.cc
  DEFINES_FILE ${CMAKE_CURRENT_SOURCE_DIR}/sysy.lex.h
)
ADD_FLEX_BISON_DEPENDENCY(SysYLexer SysYParser)

# ast source
set(AST_SRC "ast_type.cc" "ast.cc")

# ast
add_library(ast OBJECT ${AST_SRC})

# parser
add_library(parser STATIC
  parser.cc
  ${FLEX_SysYLexer_OUTPUTS}
  ${BISON_SysYParser_OUTPUTS}
)
target_link_libraries(parser ast)

# semantic check
add_library(semantic_check OBJECT semantic_check.cc)

# ast to ir
add_library(ast_to_ir STATIC ast_to_ir.cc)

# frontend
add_library(frontend STATIC
  ${AST_SRC}
  parser.cc
  semantic_check.cc
  ast_to_ir.cc
  ${FLEX_SysYLexer_OUTPUTS}
  ${BISON_SysYParser_OUTPUTS}
)
target_link_libraries(frontend comm ir)

# parse_ast_tool
add_executable(parse_ast_tool
  parse_ast_tool.cc
)
target_link_libraries(parse_ast_tool frontend)

# ast_to_ir_tool
add_executable(ast_to_ir_tool
  ast_to_ir_tool.cc
)
target_link_libraries(ast_to_ir_tool frontend)

## tests
include(GoogleTest)

# ast test
add_executable(ast_test ast_test.cc ast.cc ast_type.cc)
target_link_libraries(ast_test gtest_main)
gtest_discover_tests(ast_test)

macro(add_frontend_test name)
  add_executable(
    ${name}
    ${name}.cc
  )
  target_link_libraries(
    ${name}
    gtest_main
    frontend
  )
endmacro(add_frontend_test)

add_frontend_test(sysy_lexer_test)
add_frontend_test(sysy_parser_test)
add_frontend_test(semantic_check_test)
add_frontend_test(ast_to_ir_test)

gtest_discover_tests(sysy_lexer_test)
gtest_discover_tests(sysy_parser_test)
gtest_discover_tests(semantic_check_test)
gtest_discover_tests(ast_to_ir_test)
