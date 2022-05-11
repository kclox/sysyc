#ifndef __bultin_hpp__
#define __bultin_hpp__

#include "ast/ast.hpp"
#include "ir/ir.hpp"
#include <map>

extern std::map<std::string, ast::FuncDecl *> ast_builtin_funcs;
extern std::map<std::string, std::string> link_name_map;

#endif
