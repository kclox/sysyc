#include "parser.h"

int main(int argc, char **argv) {
    g_debug_enabed = true;
    FILE *fin = stdin;
    if (argc > 1)
        fin = fopen(argv[1], "r");
    ir::Module m;
    Lexer lexer(fin);
    Parser parser(lexer, m);
    if (argc > 1)
        lexer.loc.filename = std::string(strdup(argv[1]));
    parser.Parse();
    Debug("+dump\n");
    m.dump(std::cout);
    Debug("-dump\n");
    return 0;
}