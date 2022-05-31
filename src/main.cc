#include "frontend.h"
#include "opt/opt.h"
#include "backend.h"
#include "dbg.hpp"
#include <assert.h>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <unistd.h>

enum Option {
    AS = 0x100,
    LD,
    DBG,
};

int emit_ir = 0, use_clang = 0, list_opt = 0, enable_all_opt = 0, g_verbose = 0,
    emit_asm = 0, disable_ra = 0, disable_backend = 0;
static struct option long_options[] = {
    {"help", no_argument, nullptr, 'h'},
    {"func", required_argument, nullptr, 'f'},
    {"out", required_argument, nullptr, 'o'},
    {"emit-ir", no_argument, &emit_ir, 1},
    {"ir", required_argument, nullptr, 'i'},
    {"clang", no_argument, &use_clang, 1},
    {"lib-dir", required_argument, nullptr, 'L'},
    {"opt", required_argument, nullptr, 'O'},
    {"enable-all-opt", no_argument, &enable_all_opt, 1},
    {"list-opt", no_argument, &list_opt, 1},
    {"verbose", no_argument, nullptr, 'v'},
    {"emit-asm", no_argument, &emit_asm, 1},
    {"asm", required_argument, nullptr, 'a'},
    {"as", required_argument, nullptr, AS},
    {"ld", required_argument, nullptr, LD},
    {"disable-ra", no_argument, &disable_ra, 1},
    {"disable-backend", no_argument, &disable_backend, 1},
    {"dbg", required_argument, nullptr, DBG},
};

#define opt_count (sizeof(long_options) / sizeof(struct option))

char *lib_dir = nullptr;
#define IS(x) (!strcmp(long_options[i].name, x))

void help() {    //  --help  输出所有选项
    fprintf(stderr, "syscc ");
    for (int i = 0; i < opt_count; i++) {
        fprintf(stderr, "[--%s", long_options[i].name);
        if (long_options[i].has_arg) {
            if (IS("func")) {
                fprintf(stderr, " function] ");
            } else if (IS("ir")) {
                fprintf(stderr, " ir_output_file] ");
            } else {
                fprintf(stderr, " ...] ");
            }
        } else {
            fprintf(stderr, "] ");
        }
    }
    fprintf(stderr, "\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    int opt;
    const char *out_file_name = "a.out";
    FILE *fout = nullptr, *fin = stdin;
    std::ofstream f_ir_out;
    std::ostream *ir_out = &std::cout;
    std::ofstream f_asm_out;
    std::ostream *asm_out = &std::cout;
    std::vector<std::string> opts;   // 优化选项队列
    const char *func = nullptr;
    const char *as = "arm-linux-gnueabi-as -mthumb";
    const char *ld = "arm-linux-gnueabi-gcc -static";
    char buf[0x500];
    char tmp_asm_file_name[0x100];
    std::ofstream tmp_asm_out;
    int err;
    ast::CompUnit *comp_unit;
    ir::Module *m;
    backend::Asm code;
    int opt_idx;

    while ((opt = getopt_long(argc, argv, "o:f:L:O:Sa:hv", long_options,
                              &opt_idx)) != -1) {   // 读入选项
        switch (opt) {
        case 0:
            break;
        case 'o': {   // out 
            out_file_name = strdup(optarg);
        } break;
        case 'f':
            func = optarg;
            break;
        case 'i': // ir   
            if (emit_ir) {   // 参数非空
                f_ir_out.open(std::string(optarg));  // 打开写入文件
                if (!f_ir_out.is_open()) {
                    fprintf(stderr, "can not open %s for write\n", optarg);
                    return -1;
                }
                ir_out = &f_ir_out;
            }
            break;
        case 'L': {   // 链接文件
            lib_dir = strdup(optarg);
        } break;
        case 'O': {   // 优化选项
            opts.push_back(std::string(strdup(optarg)));
        } break;
        case 'v': {     // ？
            g_verbose++;
            opt::log_level = g_verbose;
        } break;
        case 'S': {
            emit_asm = 1;
        } break;
        case 'a': {   // 生成汇编
            f_asm_out.open(std::string(optarg));
            if (!f_asm_out.is_open()) {
                fprintf(stderr, "can not open %s for write\n", optarg);
                return -1;
            }
            asm_out = &f_asm_out;
        } break;
        case AS: {
            as = strdup(optarg);
        } break;
        case LD: {
            ld = strdup(optarg);
        } break;
        case DBG: {
            EnableDbg(std::string(strdup(optarg)));
        } break;
        default:
            help();
        }
    }

    if (list_opt) {    // 打印编译选项
        for (int i = 0; i < opt::mgr::module_count; i++) {
            fprintf(stderr, "%s\n", opt::mgr::modules[i].name.c_str());
        }
        exit(0);
    }

    if (enable_all_opt) {    // 所有优化
        for (int i = 0; i < opt::mgr::module_count; i++) {
            opts.push_back(opt::mgr::modules[i].name);
        }
    }


    if (optind < argc) {    // 处理输出文件（sys）
        fin = fopen(argv[optind], "r");
        if (fin == nullptr) {
            fprintf(stderr, "can not open %s\n", argv[optind]);
            return -1;
        }
    }

    // 若没有定义链接文件，则链接默认路径文件  build/lib
    // fill lib paths
    char lib_dir_buf[0x200];
    if (!lib_dir) {
        // char sysycc_path[0x100];
        // readlink(argv[0], sysycc_path, 0x100 - 1);
        snprintf(lib_dir_buf, 0x200 - 1, "%s/../lib", argv[0]);   
        lib_dir = lib_dir_buf;
    }

    comp_unit = Parse(fin);
    comp_unit = SemanticCheck(comp_unit);   // 语义检查
    m = AstToIr(comp_unit);   // 语法树转IR
    for (auto opt : opts) {    // IR优化
        opt::mgr::RunOpt(opt, *m);
    }
    if (emit_ir) {   // 在终端打印
        m->dump(*ir_out);
    }
    if (use_clang) {
        char tmp_ir_file_name[0x100];
        snprintf(tmp_ir_file_name, 0x100 - 1, "/tmp/%d.ll", rand());
        std::ofstream tmp_ir_out;
        tmp_ir_out.open(std::string(tmp_ir_file_name));
        m->dump(tmp_ir_out);
        tmp_ir_out.close();
        snprintf(buf, 0x500 - 1, "clang -O0 '%s' -m32 -o '%s' '%s/libsysy.a'",
                 tmp_ir_file_name, out_file_name, lib_dir);
        int err = system(buf);
        unlink(tmp_ir_file_name);
        return WEXITSTATUS(err);
    }

    if (disable_backend) {   // 禁止后端
        goto _exit;
    }

    backend::IrToAsm(*m, code, disable_ra, emit_asm);   // IR to ARM  ///////
    if (emit_asm) {
        code.dump(*asm_out);
        goto _exit;
    }

    if (disable_ra)
        goto _exit;

    snprintf(tmp_asm_file_name, 0x100 - 1, "/tmp/%d.s", rand());
    tmp_asm_out.open(std::string(tmp_asm_file_name));
    code.dump(tmp_asm_out);
    tmp_asm_out.close();
    snprintf(buf, 0x500 - 1, "%s -o '/tmp/tmp.out' '%s'", as,
             tmp_asm_file_name);
    err = WEXITSTATUS(system(buf));
    unlink(tmp_asm_file_name);
    if (err != 0)
        goto _exit;
    snprintf(buf, 0x500 - 1, "%s -o '%s' '/tmp/tmp.out' '%s/libsysy.a'", ld,
             out_file_name, lib_dir);
    err = WEXITSTATUS(system(buf));
    unlink("/tmp/tmp.out");

_exit:
    //　关闭流
    if (fout && fout != stdout)
        fclose(fout);
    if (fin && fin != stdin)
        fclose(fin);
    return err;
}
