%{
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "ast/ast.hpp"
#include "sysy.tab.h"
void yyerror(const char *);
%}

%x BlockComment
%%
<BlockComment>[^*\n]*        /* eat anything that's not a '*' */
<BlockComment>"*"+[^*/\n]*   /* eat up '*'s not followed by '/'s */
<BlockComment>\n
<BlockComment>"*"+"/"   { BEGIN(INITIAL); }
\/\*                    { BEGIN(BlockComment); }
\/\/.*

const       { return CONST; }
int         { return INT; }
void        { return VOID; }
while       { return WHILE; }
break       { return BREAK; }
continue    { return CONTINUE; }
if          { return IF; }
else        { return ELSE; }
return      { return RET; }

[a-zA-Z_][a-zA-Z0-9_]*  { yylval.ident = strdup(yytext); return IDENT; }
0[0-9a-fA-F]+           { yylval.integer = (int)strtol(yytext, nullptr, 8); return INTEGER; }
[0-9]+                  { yylval.integer = atoi(yytext); return INTEGER; }
0x[0-9a-fA-F]+          { yylval.integer = (int)strtol(yytext, nullptr, 16); return INTEGER; }

==          { return EQ; }
!=          { return NE; }
\>\=        { return GE; }
\<\=        { return LE; }
\&\&        { return LAND; }
\|\|        { return LOR; }
[=+\-\*/;\(\)\{\}\<\>\[\],\%\!]     return *yytext;
[ \t\n]     ;
.           yyerror("invalid character");
%%

int yywrap() {
    return 1;
}
