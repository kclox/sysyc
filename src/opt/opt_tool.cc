#include "../ir/parser.h"
#include "opt/opt.h"

int main(int argc, char **argv) {
    g_debug_enabed = false;
    FILE *fin = stdin;
    if (argc > 1)
        fin = fopen(argv[1], "r");
    ir::Module m;
    Lexer lexer(fin);
    Parser parser(lexer, m);
    if (argc > 1)
        lexer.loc.filename = std::string(strdup(argv[1]));
    parser.Parse();
    opt::InitBBPtr(m);
    opt::BuildDTree(m);
    opt::Mem2Reg(m);
    opt::ConstantOpt(m);
    //增加全局值编号
    opt::gvn(m);
    //增加零一消除
    opt::Zero_One_Elimination(m);
    //增加活跃变量分析
    opt::Active_Variable_Analysis(m);
    //在活跃变量分析的基础上进行死代码删除
    opt::Dead_Code_Elimination(m);
    m.dump(std::cout);
    return 0;
}