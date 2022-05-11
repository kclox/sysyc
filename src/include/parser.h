#ifndef __parser_h__
#define __parser_h__

#include "ast/ast.hpp"

ast::CompUnit *Parse(FILE *fin);
ast::CompUnit *Parse(const char *buf, int size);

extern ast::CompUnit *comp_unit;

#endif
