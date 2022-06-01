#include "backend.h"

#include <stdarg.h>
#include "dbg.hpp"

namespace backend {

bool NotAllowTy;

void Asm::dump(std::ostream &os) {
    os << ".data\n";

    for (auto &def : defs) {
        if (!def->is_const)
            os << def->str() << '\n';
    }
    os << "\n";

    os << ".text\n"
          ".global\tmain\n" // main
          ".syntax unified\n"
          ".code\t16\n"
          ".thumb_func\n"
          ".fpu softvfp\n"
          ".type\tmain, %function\n"; // main
    for (auto &f : funcs) {
        f->dump(os);
        os << "\n";
    }

    os << "\n";

    for (auto &def : defs) {
        if (def->is_const)
            os << def->str() << '\n';
    }
}

void IrToAsm(ir::Module &m, Asm &_asm, bool disable_ra, bool emit_asm) {
    NotAllowTy = emit_asm;
    InstrSelect(m, _asm);
    if (!disable_ra)
        RegAlloca(_asm);
}

}; // namespace backend
