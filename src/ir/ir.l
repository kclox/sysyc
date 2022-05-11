%{
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
void yyerror(const char *);
#include "token.h"
#include "location.h"
extern Token token;
extern Location g_ir_l_location;
%}

%%
;[^\n]*                         ;

[0-9]+\:                        { token.data = atoi(yytext); return Token::kTmpLabel; }
[a-zA-Z_\.][a-zA-Z0-9_\.]*\:    { token.data = std::string(strdup(yytext), strlen(yytext) - 1); return Token::kNamedLabel; }


declare         { return Token::kDeclare; }
define          { return Token::kDefine; }
global          { return Token::kGlobal; }
align           { return Token::kAlign; }
to              { return Token::kTo; }
zeroinitializer { return Token::kZeroinit; }
constant        { return Token::kConst; }

i32             { return Token::kI32; }
i1              { return Token::kI1; }
void            { return Token::kVoid; }
label           { return Token::kLabelT; }

call            { return Token::kCall; }
ret             { return Token::kRet; }
br              { return Token::kBr; }
icmp            { return Token::kIcmp; }
add             { return Token::kAdd; }
sub             { return Token::kSub; }
mul             { return Token::kMul; }
sdiv            { return Token::kSdiv; }
srem            { return Token::kSrem; }
and             { return Token::kAnd; }
or              { return Token::kOr; }
load            { return Token::kLoad; }
store           { return Token::kStore; }
alloca          { return Token::kAlloca; }
phi             { return Token::kPhi; }
getelementptr   { return Token::kGetelementptr; }
zext            { return Token::kZext; }
bitcast         { return Token::kBitcast; }
eq              { return Token::kCondEq; }
ne              { return Token::kCondNe; }
ugt             { return Token::kCondUgt; }
uge             { return Token::kCondUge; }
ule             { return Token::kCondUle; }
sgt             { return Token::kCondSgt; }
sge             { return Token::kCondSge; }
slt             { return Token::kCondSlt; }
sle             { return Token::kCondSle; }

\@[a-zA-Z_\.][a-zA-Z0-9_\.]*    { token.data = std::string(strdup(&yytext[1])); return Token::kGlobalVar; }
\%[a-zA-Z_\.][a-zA-Z0-9_\.]*    { token.data = std::string(strdup(&yytext[1])); return Token::kLocalVar; }
\%[0-9]+                        { token.data = atoi(&yytext[1]); return Token::kTmpVar; }
[+-]?[0-9]+                     { token.data = atoi(yytext); return Token::kInteger; }
[+-]?0x[0-9a-fA-F]+             { token.data = (int)strtol(yytext, nullptr, 16); return Token::kInteger; }

=               { return Token::kEq; }
x               { return Token::kX; }
\*              { return Token::kPtr; }
\[              { return Token::kLbracket; }
\]              { return Token::kRbracket; }
\(              { return Token::kLParenthesis; }
\)              { return Token::kRParenthesis; }
\{              { return Token::kLBrace; }
\}              { return Token::kRBrace; }
\,              { return Token::kComma; }

[ \t]           ;
\n              { g_ir_l_location.line++; }

dso_local               ;
noundef                 ;
source_filename[^\n]*   ;
target\ [^\n]*          ;
attributes\ [^\n]*      ;
\!llvm[^\n]*            ;
\![0-9]+[^\n]*          ;
\#[0-9]+                ;

.               return -1;
%%

Token token;
Location g_ir_l_location;

int yywrap() {
    return 1;
}
