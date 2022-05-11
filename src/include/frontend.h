#ifndef __frontend_h__
#define __frontend_h__

#include "ast/ast.hpp"
#include "ir/ir.hpp"
#include "parser.h"

extern ast::CompUnit *comp_unit;

ast::CompUnit *SemanticCheck(ast::CompUnit *comp_unit);
ir::Module *AstToIr(ast::CompUnit *comp_unit);

#endif