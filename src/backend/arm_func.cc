#include "backend.h"
#include <stdarg.h>

namespace backend {

/************   BB   ***************/

void BB::dump(std::ostream &os) {
    if (label.length() > 0)
        os << label << ":\n";
    for (auto &inst : insts) {
        os << "    " << inst->str() << "\n";
    }
    if (branch)
        os << "    " << branch->str() << "\n";
}
void BB::SetBranch(Cond cond, std::string label) {
    branch = std::make_unique<Branch>(cond, label);
}
/************   BB   ***************/

/************   StackFrame   ***************/
void StackFrame ::Dump(std::ostream &os) {
    os << "[sp, 0] -------------------------------\n"
       << "\t" << max_call_arg_count * 4 << " (call arguments reserve area)\n"
       << "--------------------------------------\n"
       << "\t"
       << "16 (r0-r4)\n"
       << "[" << Reg2Str(local_var_base)
       << ", 0] -------------------------------\n"
       << "\t" << local_var_size << " (local variable area)\n"
       << "[" << Reg2Str(local_var_base) << ", " << local_var_size
       << "] -------------------------------\n"
       << "\t" << 9 * 4 << " (r0-r7, lr)\n"
       << "--------------------------------------\n"
       << spilled_arg_count * 4 << " (spilled arguments)\n"
       << "--------------------------------------\n";
}

inline int StackFrame ::AlignHigh(int offset, int alignment) {
    return offset;
    // if (offset == 0)
    //     return offset;
    // return ((offset - 1) / alignment + 1) * alignment;
}
int StackFrame ::AllocaVar(int size, int alignment) {
    int offset = AlignHigh(local_var_size, alignment);
    local_var_size = offset + size;
    return offset;
}

Address StackFrame ::CallArgAddr(int idx) {
    assert(idx >= max_reg_arg_count);
    Address addr;
    addr.mode = Address::kMBaseImm;
    addr.base = Reg::SP;
    addr.offset.imm = (idx - max_reg_arg_count) * 4;
    return addr;
}

Address StackFrame ::BackupAddress(Reg r) {
    assert((int)r < max_reg_arg_count);
    Address addr;
    addr.mode = Address::kMBaseImm;
    addr.base = Reg::SP;
    addr.offset.imm = (max_call_arg_count + (int)r) * 4;
    return addr;
}

Address StackFrame ::VarAddr(int offset) {
    Address addr;
    addr.mode = Address::kMBaseImm;
    addr.base = local_var_base;
    addr.offset.imm = offset;
    return addr;
}

// r7 +
inline int StackFrame ::SpilledArgOffset() {
    return local_var_size + SaveRegSize();
}

// r7 +
int StackFrame ::ArgOffset(int idx) {
    if (idx < max_reg_arg_count)
        return local_var_size + idx * 4;
    else
        return SpilledArgOffset() + (idx - max_reg_arg_count) * 4;
}

Address StackFrame ::ArgAddr(int idx) {
    Address addr;
    addr.mode = Address::kMBaseImm;
    addr.base = local_var_base;
    addr.offset.imm = ArgOffset(idx);
    return addr;
}
/************   StackFrame   ***************/

/************   func   ***************/
void Func ::dump(std::ostream &os) {
    os << "@ function: " << name << ", argc: " << args.size()
       << ", ret: " << has_ret << '\n';
    if (cmt)
        os << cmt.str() << '\n';

    os << name << ":\n";

    entry.dump(os);
    for (auto &bb : bbs) {
        bb->dump(os);
    }
    end.dump(os);
    for (auto &i : imms) {
        os << i.first << ":\n" << i.second->str() << "\n";
    }
}

std::string Func ::CreateIntImm(Word v) {
    auto it = imms.find(v);
    if (it != imms.end())
        return it->second->label;
    std::string label;
    auto imm = std::make_unique<Imm>();
    imm->v = v;
    imm->ty = ".int";
    label = "__const_" + name + "_" + std::to_string(v);
    imm->label = label;
    imms.insert({v, std::move(imm)});
    return label;
}

Reg Func ::AllocaVReg() { return (Reg)vreg++; }

void Func ::ResetBBID() {
    int i = 0;
    for (auto &bb : bbs)
        bb->id = i++;
}

/************   func   ***************/

}; // namespace backend