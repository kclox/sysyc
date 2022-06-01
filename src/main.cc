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
    OPT,
    ENABLE_ALL_OPT,
};

int emit_ir = 0, use_clang = 0, list_opt = 0, enable_all_opt = 0, g_verbose = 0,
    emit_asm = 0, disable_ra = 0, disable_backend = 0, opt_level = 0;
static struct option long_options[] = {
    {"help", no_argument, nullptr, 'h'},
    {"func", required_argument, nullptr, 'f'},
    {"out", required_argument, nullptr, 'o'},
    {"emit-ir", no_argument, &emit_ir, 1},
    {"ir", required_argument, nullptr, 'i'},
    {"clang", no_argument, &use_clang, 1},
    {"lib-dir", required_argument, nullptr, 'L'},
    {"opt", required_argument, nullptr, OPT},
    {"enable-all-opt", no_argument, nullptr, ENABLE_ALL_OPT},
    {"list-opt", no_argument, &list_opt, 1},
    {"verbose", no_argument, nullptr, 'v'},
    {"emit-asm", no_argument, &emit_asm, 1},
    {"asm", required_argument, nullptr, 'o'},
    {"as", required_argument, nullptr, AS},
    {"ld", required_argument, nullptr, LD},
    {"disable-ra", no_argument, &disable_ra, 1},
    {"disable-backend", no_argument, &disable_backend, 1},
    {"dbg", required_argument, nullptr, DBG},
    {"opt-level", required_argument, nullptr, 'O'},
};

#define opt_count (sizeof(long_options) / sizeof(struct option))

char *lib_dir = nullptr;

void help() {
    fprintf(stderr, "syscc ");
    for (int i = 0; i < opt_count; i++) {
        fprintf(stderr, "[--%s", long_options[i].name);
        if (long_options[i].has_arg) {
#define IS(x) (!strcmp(long_options[i].name, x))
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
    bool has_custom_output = false;
    const char *out_file_name = "a.out";
    FILE *fout = nullptr, *fin = stdin;
    std::ofstream f_ir_out;
    std::ostream *ir_out = &std::cout;
    std::ofstream f_asm_out;
    std::ostream *asm_out = &std::cout;
    std::vector<std::string> opts;
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
                              &opt_idx)) != -1) {
        switch (opt) {
        case 0:
            break;
        case 'o': {
            out_file_name = strdup(optarg);
            has_custom_output = true;
        } break;
        case 'f':
            func = optarg;
            break;
        case 'i': // ir
            if (emit_ir) {
                f_ir_out.open(std::string(optarg));
                if (!f_ir_out.is_open()) {
                    fprintf(stderr, "can not open %s for write\n", optarg);
                    return -1;
                }
                ir_out = &f_ir_out;
            }
            break;
        case 'L': {
            lib_dir = strdup(optarg);
        } break;
        case OPT: {
            opts.push_back(std::string(strdup(optarg)));
        } break;
        case 'v': {
            g_verbose++;
            opt::log_level = g_verbose;
        } break;
        case 'S': {
            emit_asm = 1;
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
        case 'O': {
            opt_level = atoi(optarg);
            if (opt_level >= opt::mgr::level_count) {
                fprintf(stderr, "invalid optimization level %d, must < %d\n",
                        opt_level, opt::mgr::level_count);
                exit(1);
            }
            auto &t = opt::mgr::levels[opt_level];
            std::copy(t.begin(), t.end(), std::back_inserter(opts));
        } break;
        case ENABLE_ALL_OPT: {
            enable_all_opt = true;
            for (int i = 0; i < opt::mgr::module_count; i++) {
                opts.push_back(opt::mgr::modules[i].name);
            }
        } break;
        default:
            help();
        }
    }

    if (list_opt) {
        for (int i = 0; i < opt::mgr::module_count; i++) {
            fprintf(stderr, "%s\n", opt::mgr::modules[i].name.c_str());
        }
        exit(0);
    }

    if (optind < argc) {
        fin = fopen(argv[optind], "r");
        if (fin == nullptr) {
            fprintf(stderr, "can not open %s\n", argv[optind]);
            return -1;
        }
    }

    // fill lib paths
    char lib_dir_buf[0x200];
    if (!lib_dir) {
        // char sysycc_path[0x100];
        // readlink(argv[0], sysycc_path, 0x100 - 1);
        snprintf(lib_dir_buf, 0x200 - 1, "%s/../lib", argv[0]);
        lib_dir = lib_dir_buf;
    }

    comp_unit = Parse(fin);
    comp_unit = SemanticCheck(comp_unit);
    m = AstToIr(comp_unit);
    for (auto opt : opts) {
        opt::mgr::RunOpt(opt, *m);
    }
    if (emit_ir) {
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

    if (disable_backend) {
        goto _exit;
    }

    backend::IrToAsm(*m, code, disable_ra, emit_asm);
    if (emit_asm) {
        if (has_custom_output) {
            f_asm_out.open(out_file_name);
            asm_out = &f_asm_out;
        }
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
    if (fout && fout != stdout)
        fclose(fout);
    if (fin && fin != stdin)
        fclose(fin);
    return err;
}