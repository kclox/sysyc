#include "ast/ast.hpp"
#include "frontend.h"
#include <iostream>
#include <fstream>

int main(int argc, char *argv[]) {
    FILE *fin = stdin;
    if (argc > 1)
        fin = fopen(argv[1], "r");
    ast::CompUnit *u = Parse(fin);
    u = SemanticCheck(u);
    std::ofstream fout;
    if (argc > 2)
        fout.open(argv[2]);
    if (fout.is_open())
        fout << u->to_json();
    else
        std::cout << u->to_json().dump(2);
    return 0;
}
