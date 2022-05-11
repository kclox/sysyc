/* A Bison parser, made by GNU Bison 3.5.1.  */

/* Skeleton interface for Bison GLR parsers in C

   Copyright (C) 2002-2015, 2018-2020 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_YY_HOME_LT_PROJECTS_SYSYCOMPILER_SRC_FRONTEND_SYSY_TAB_H_INCLUDED
# define YY_YY_HOME_LT_PROJECTS_SYSYCOMPILER_SRC_FRONTEND_SYSY_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    INTEGER = 258,
    IDENT = 259,
    INT = 260,
    CONST = 261,
    VOID = 262,
    WHILE = 263,
    BREAK = 264,
    CONTINUE = 265,
    IF = 266,
    ELSE = 267,
    RET = 268,
    EQ = 269,
    NE = 270,
    LE = 271,
    GE = 272,
    LAND = 273,
    LOR = 274,
    NOT = 275,
    UMINUS = 276,
    UADD = 277
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 11 "/home/lt/projects/SysYCompiler/src/frontend/sysy.yy"

    int integer;
    char *ident;
    ast::AstNode *ast;
    ast::Type* type;
    List<ast::Decl*> *decls;
    ast::Decl *decl;
    ast::ArrayInitVal *array_init_val;
    List<ast::Exp*> *exp_list;
    ast::Exp *exp;
    ast::CompUnit *comp_unit;
    ast::Func *func;
    ast::Array *array;
    ast::BlockStmt *block;
    List<ast::Stmt*> *stmt_list;
    ast::Stmt *stmt;
    List<ast::AstNode*> *asts;
    List<ast::ArrayInitItem*> *array_init_list;
    ast::ArrayInitItem *array_init_item;

#line 97 "/home/lt/projects/SysYCompiler/src/frontend/sysy.tab.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_HOME_LT_PROJECTS_SYSYCOMPILER_SRC_FRONTEND_SYSY_TAB_H_INCLUDED  */
