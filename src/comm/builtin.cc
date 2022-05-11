#include "builtin.hpp"
#include "ast/type.h"

auto ts = &ast::g_type_system;

std::map<std::string, ast::FuncDecl *> ast_builtin_funcs = {
    {"getint", new ast::FuncDecl("getint", ts->TInt(), {})},
    {"getch", new ast::FuncDecl("getch", ts->TInt(), {})},
    {"getarray",
     new ast::FuncDecl("getarray", ts->TInt(), {ts->TArray(ts->TInt(), {0})})},
    {"putint", new ast::FuncDecl("putint", ts->TVoid(), {ts->TInt()})},
    {"putch", new ast::FuncDecl("putch", ts->TVoid(), {ts->TInt()})},
    {"putarray", new ast::FuncDecl("putarray", ts->TVoid(),
                                   {ts->TInt(), ts->TArray(ts->TInt(), {0})})},
    {"starttime", new ast::FuncDecl("_sysy_starttime", ts->TVoid(), {})},
    {"stoptime", new ast::FuncDecl("_sysy_starttime", ts->TVoid(), {})},
    {"set_i32_array",
     new ast::FuncDecl("_sysy_set_i32_array", ts->TVoid(),
                       {ts->TInt(), ts->TArray(ts->TInt(), {0}), ts->TInt()})},
};

std::map<std::string, std::string> link_name_map = {
    {"starttime", "_sysy_starttime"},
    {"stoptime", "_sysy_stoptime"},
    {"set_i32_array", "_sysy_set_i32_array"},
};