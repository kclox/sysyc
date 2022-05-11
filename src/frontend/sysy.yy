%{
#include <stdio.h>
#include "ast/ast.hpp"
int yylex();
void yyerror(const char *s);
extern FILE *yyin;
using namespace ast;
extern ast::CompUnit *comp_unit;
%}

%union {
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
};

%glr-parser

%type <type> BType FuncType
%type <decls> Decl ConstDecl VarDecl DefWithInit VarDef FuncFParams FuncFParamsNotEmpty
%type <array_init_val> ArrayInitVal
%type <array> ArrayRef
%type <exp_list> ExpList ExpListNotEmpty
%type <exp> Exp
%type <array_init_list> ArrayInitValList
%type <comp_unit> CompUnit
%type <func> FuncDef
%type <decl> FuncFParam VarDef1
%type <block> Block
%type <stmt_list> StmtList
%type <stmt> Stmt StmtNoIf If1 If2
%type <array_init_item> ArrayInitItem

%token <integer> INTEGER
%token <ident> IDENT

%token INT
%token CONST
%token VOID

%token WHILE
%token BREAK
%token CONTINUE
%token IF
%token ELSE
%token RET

// +-*/%<>
%token EQ
%token NE
%token LE
%token GE
// !
%token LAND
%token LOR

%left ','
%right '='
%left LOR
%left LAND
%left EQ NE
%left '<' '>' GE LE
%left '+' '-'
%left '*' '/' '%'
%nonassoc NOT UMINUS UADD

%%

/* Compile Unit */
CompUnit:               { comp_unit = new CompUnit(); }
    | CompUnit Decl     { for (auto x : *$2) comp_unit->decls.insert(x->name, x); delete $2; }
    | CompUnit FuncDef  { comp_unit->funcs.insert($2->name, $2); }
    ;

/* Type */
BType: INT              { $$ = ast::g_type_system.TInt(); }
    ;
FuncType: VOID          { $$ = ast::g_type_system.TVoid(); } 
    | BType             { $$ = $1; }
    ;

/* Declaration */
Decl: ConstDecl         { $$ = $1; }
    | VarDecl           { $$ = $1; }
    ;

// Const declaration
ConstDecl: CONST BType DefWithInit ';'  { $$ = $3; for (auto x : *$3) x->SetAttr($2, true); }
    ;

DefWithInit: IDENT '=' Exp              { $$ = new List<Decl*>(new BDecl(nullptr, $1, $3)); }
    | IDENT ArrayRef '=' ArrayInitVal   { $$ = new List<Decl*>(new ArrayDecl(nullptr, $1, $2, $4)); }
    | DefWithInit ',' DefWithInit       { $$ = $1->Append($3); delete $3; }
    ;

ArrayRef: '[' ']'                       { $$ = new Array(nullptr); }
    | '[' Exp ']'                       { $$ = new Array($2); }
    | ArrayRef '[' Exp ']'              { $$ = $1; $1->dimensions.push_back($3); }
    ;

ArrayInitVal: '{' '}'                   { $$ = new ArrayInitVal(); }
    | '{' ArrayInitValList '}'          { $$ = new ArrayInitVal(*$2); }
    ;

ArrayInitValList: ArrayInitItem             { $$ = new List<ArrayInitItem *>($1); }
    | ArrayInitValList ',' ArrayInitItem    { $$ = $1->Append($3); }
    ;

ArrayInitItem: Exp                      { $$ = $1; } 
    | ArrayInitVal                      { $$ = $1; }
    ;

ExpList:                                { $$ = new List<Exp*>(); }
    | ExpListNotEmpty                   { $$ = $1; }
    ;

ExpListNotEmpty: Exp                    { $$ = new List<Exp*>($1); }
    | ExpListNotEmpty ',' Exp           { $$ = $1->Append($3); }
    ;

// Variable Declaration
VarDecl: BType VarDef ';'               { $$ = $2; for (auto x : *$2) x->SetAttr($1, false); }
    ;

VarDef: VarDef1                         { $$ = new List<Decl*>($1); }
    | VarDef ',' VarDef1                { $$ = $1->Append($3); }
    ;

VarDef1: IDENT                          { $$ = new BDecl(nullptr, $1); }
    | IDENT ArrayRef                    { $$ = new ArrayDecl(nullptr, $1, $2); }
    | IDENT '=' Exp                     { $$ = new BDecl(nullptr, $1, $3); }
    | IDENT ArrayRef '=' ArrayInitVal   { $$ = new ArrayDecl(nullptr, $1, $2, $4); }
    ;

/* Function Defination */
FuncDef: FuncType IDENT '(' FuncFParams ')' Block   { $$ = new Func(std::string($2), $1, *$4, $6); delete $4; }
    ;

FuncFParams:                                { $$ = new List<Decl*>(); }
    | FuncFParamsNotEmpty                   { $$ = $1; }
    ;

FuncFParamsNotEmpty: FuncFParam             { $$ = new List<Decl*>($1); }
    | FuncFParamsNotEmpty ',' FuncFParam    { $$ = $1->Append($3); }
    ;

FuncFParam: BType IDENT                     { $$ = new BDecl($1, std::string($2)); }
    | BType IDENT ArrayRef                  { $$ = new ArrayDecl($1, std::string($2), $3); }
    ;

/* Expression */
Exp: INTEGER                { $$ = new ConstExp($1); }
    | IDENT                 { $$ = new VarExp(std::string(strdup($1))); }
    | Exp '+' Exp           { $$ = new BinaryExp(AstNode::kOpAdd, $1, $3); }
    | Exp '-' Exp           { $$ = new BinaryExp(AstNode::kOpSub, $1, $3); }
    | Exp '*' Exp           { $$ = new BinaryExp(AstNode::kOpMul, $1, $3); }
    | Exp '/' Exp           { $$ = new BinaryExp(AstNode::kOpDiv, $1, $3); }
    | Exp '%' Exp           { $$ = new BinaryExp(AstNode::kOpMod, $1, $3); }
    | Exp '<' Exp           { $$ = new BinaryExp(AstNode::kOpLt, $1, $3); }
    | Exp '>' Exp           { $$ = new BinaryExp(AstNode::kOpGt, $1, $3); }
    | Exp EQ Exp            { $$ = new BinaryExp(AstNode::kOpEq, $1, $3); }
    | Exp NE Exp            { $$ = new BinaryExp(AstNode::kOpNe, $1, $3); }
    | Exp LE Exp            { $$ = new BinaryExp(AstNode::kOpLe, $1, $3); }
    | Exp GE Exp            { $$ = new BinaryExp(AstNode::kOpGe, $1, $3); }
    | '!' Exp %prec NOT     { $$ = new UnaryExp(AstNode::kOpNot, $2); }
    | '-' Exp %prec UMINUS  { $$ = new UnaryExp(AstNode::kOpUMinus, $2); }
    | '+' Exp %prec UADD    { $$ = new UnaryExp(AstNode::kOpUAdd, $2); }
    | Exp LAND Exp          { $$ = new BinaryExp(AstNode::kOpLAnd, $1, $3); }
    | Exp LOR Exp           { $$ = new BinaryExp(AstNode::kOpLOr, $1, $3); }
    | IDENT '(' ExpList ')' { $$ = new CallExp(std::string(strdup($1)), *$3); delete $3; }
    | IDENT ArrayRef        { $$ = new RefExp(std::string(strdup($1)), $2); }
    | '(' Exp ')'           { $$ = $2; }
    ;

/* Statement */
Stmt: If1                   { $$ = $1; }
    | If2                   { $$ = $1; }
    ;

If1: IF '(' Exp ')' If1 ELSE If1    { $$ = new IfStmt($3, $5, $7); }
    | StmtNoIf                      { $$ = $1; }
    ;

If2: IF '(' Exp ')' Stmt            { $$ = new IfStmt($3, $5); }
    | IF '(' Exp ')' If1 ELSE If2   { $$ = new IfStmt($3, $5, $7); }
    ;

StmtNoIf: Exp '=' Exp ';'           { $$ = new AssignStmt($1, $3); }
    | Decl                          { $$ = new DeclStmt(*$1); }
    | WHILE '(' Exp ')' Stmt        { $$ = new WhileStmt($3, $5); }
    | BREAK ';'                     { $$ = new BreakStmt(); }
    | CONTINUE ';'                  { $$ = new ContinueStmt(); }
    | RET ';'                       { $$ = new ReturnStmt(); }
    | RET Exp ';'                   { $$ = new ReturnStmt($2); }
    | Block                         { $$ = $1; }
    | Exp ';'                       { $$ = new ExpStmt($1); }
    | ';'                           { $$ = new NopStmt(); }
    ;

// Block
Block: '{' StmtList '}'     { $$ = new BlockStmt(*$2); delete $2; }
    ;

StmtList:                   { $$ = new List<Stmt*>(); }
    | StmtList Stmt         { $$ = $$->Append($2); }
    ;

%%

CompUnit *comp_unit = nullptr;

void yyerror(const char *msg) {
    fprintf(stderr, "[yyerror] %s\n", msg);
}
