#include "parser.h"
#include "sysy.lex.h"
#include "sysy.tab.h"

using namespace ast;

CompUnit *Parse(FILE *fin) {
    yyin = fin;
    yyparse();
    return comp_unit;
}

CompUnit *Parse(const char *buf, int size) {
    char *p = new char[size + 2];
    p[size] = 0, p[size + 1] = 0;
    memcpy(p, buf, size);
    YY_BUFFER_STATE s = yy_scan_buffer(p, size + 2);
    yyparse();
    yy_delete_buffer(s);
    delete[] p;
    return comp_unit;
}
