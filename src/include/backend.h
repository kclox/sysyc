#ifndef __backend_h__
#define __backend_h__

#include "ir/ir.hpp"
#include <string>
#include <vector>
#include <memory>
#include <list>

namespace backend {
enum struct Reg {
    R0,
    R1,
    R2,
    R3,
    R4,
    R5,
    R6,
    R7,
    R8,
    R9,
    R10,
    R11,
    FP = R11,
    R12,
    R13,
    SP,
    LR,
    PC,
    VREG,
    INVALID = -1,
};

std::string Reg2Str(Reg r);

typedef unsigned Word;

struct Shift {
    int type;
    int imm;
    enum { kLSL };

    std::string str() const {
        static std::string m[]{"LSL"};
        return m[type] + " #" + std::to_string(imm);
    }
};

struct Cond {
    int type;
    enum { EQ, NE, HI, HS, LS, LO, GT, GE, LT, LE, AL };
    Cond() : type(AL) {}
    Cond(int type) : type(type) {}

    std::string str() const {
        static std::string m[]{"EQ", "NE", "HI", "HS", "LS", "LO",
                               "GT", "GE", "LT", "LE", ""};
        return m[type];
    }

    Cond Not() {
        static std::map<int, int> m{
            {EQ, NE}, {NE, EQ}, {HI, LS}, {HS, LO}, {LO, HS},
            {LS, HI}, {GT, LE}, {GE, LT}, {LT, GE}, {LE, GT},
        };
        return Cond(m[type]);
    }
};

struct Address {
    // [<Rn>, <offset>]
    // [<Rn>, <offset>]!
    // [<Rn>], <offset>
    // <offset>:
    //  <imm8> <imm12>
    //  <Rm>
    //  <Rm>, <sr_type> #<shift>
    // <sr_type>:
    //  ASR, LSL, LSR, ROR
    // <shift>:
    //  5 immediate bits
    int mode;
    enum {
        // | 1            | 0          |
        // | enable index | index type |
        PreIndex = 2,
        PostIndex = 3,

        // | - 3  2 |
        // | mode   |
        kMBase = 1 << 2,
        kMBaseImm = 2 << 2,
        kMBaseReg = 3 << 2,
        kMBaseRegShift = 4 << 2,
        KMLabel = 5 << 2, // =<label> -> [pc, #imm]
    };
    const static int mode_index_mask = 0b11;
    const static int mode_m_mask = 0b11100;
    Reg base;
    struct {
        unsigned imm;
        Reg reg;
        Shift shift;
    } offset;
    std::string label;

    Address() {}
    Address(int mode) : mode(mode) {}
    Address(Reg base) {
        mode = kMBase;
        this->base = base;
    }
    Address(std::string label) {
        mode = KMLabel;
        this->label = label;
    }

    std::string str() const {
        std::string s_offset;
        switch (mode & mode_m_mask) {
        case kMBase:
            return "[" + Reg2Str(base) + "]";
        case kMBaseImm:
            s_offset += "#" + std::to_string(offset.imm);
            break;
        case kMBaseReg:
            s_offset += Reg2Str(offset.reg);
            break;
        case kMBaseRegShift:
            s_offset += Reg2Str(offset.reg) + ", " + offset.shift.str();
            break;
        case KMLabel:
            return label;
        }

        switch (mode & mode_index_mask) {
        case PreIndex:
            return "[" + Reg2Str(base) + ", " + s_offset + "]" + "!";
        case PostIndex:
            return "[" + Reg2Str(base) + "], " + s_offset;
        default:
            return "[" + Reg2Str(base) + ", " + s_offset + "]";
        }
    }

    std::vector<Reg *> RRegs() {
        std::vector<Reg *> regs;
        switch (mode & mode_m_mask) {
        case kMBase:
        case kMBaseImm:
            regs.push_back(&base);
            break;
        case kMBaseReg:
        case kMBaseRegShift:
            regs.push_back(&base);
            regs.push_back(&offset.reg);
            break;
        }
        return regs;
    }
};

typedef union {
    Reg r;
    Word imm;
} RegImmU;

std::string RegImmU2Str(RegImmU x, int flags);

struct Line {
    virtual std::string str() const = 0;
};

struct Comment : public Line {
    std::string data;

    operator bool() const { return data.length() > 0; }

    virtual std::string str() const { return data; };
};

struct Imm : public Line {
    std::string label;
    std::string ty;
    Word v;
    virtual std::string str() const {
        return label + ":\n" + ty + " " + std::to_string(v);
    }
};

struct ImmArray : public Line {
    std::string label;
    std::string ty;
    std::vector<Word> vals;
    virtual std::string str() const {
        std::string s = label + ":\n" + ty + " ";
        for (auto v : vals)
            s += v + " ";
        return s;
    }
};

struct Instr : public Line {
    int op;
    enum {
        kLDR,
        kSTR,
        kPUSH,
        kPOP,
        kADR,

        kB,
        kBL,

        kADD,
        kSUB,
        kMUL,
        kSDIV,
        kUDIV,
        kAND,
        kOR,
        kADC,
        kLSL,
        kLSR,

        kMOV,
        kNEG,

        kCMP,

        kCall,
        kLabel,
    };

    int flags;
    enum {
        kFlagS = 1,
        kFlagBIsImm = 2,
        kFlagHasShift = 4,
    };
    Comment cmt;

    Instr() { flags = 0; }
    Instr(int op) : op(op) { flags = 0; }

    virtual std::string str() const = 0;

    virtual std::vector<Reg *> RRegs() { return {}; }
    virtual std::vector<Reg *> WRegs() { return {}; };

    std::vector<Reg *> Regs() {
        auto res = RRegs();
        for (auto r : WRegs())
            res.push_back(r);
        return res;
    }

  protected:
    inline std::vector<Reg *> make_regs(Reg &r) {
        std::vector<Reg *> res;
        res.push_back(&r);
        return res;
    }
};

struct LDR : public Instr {
    Reg rd;
    Address addr;
    bool eq_addr;
    LDR() : Instr(kLDR) { eq_addr = false; }

    virtual std::string str() const {
        std::string addr_str = (eq_addr ? "=" : "") + addr.str();
        return "LDR " + Reg2Str(rd) + ", " + addr_str;
    }

    virtual std::vector<Reg *> RRegs() { return addr.RRegs(); }
    virtual std::vector<Reg *> WRegs() { return make_regs(rd); };
};

struct STR : public Instr {
    Reg rd;
    Address addr;
    STR() : Instr(kSTR) {}

    virtual std::string str() const {
        return "STR " + Reg2Str(rd) + ", " + addr.str();
    }

    virtual std::vector<Reg *> RRegs() {
        auto regs = addr.RRegs();
        regs.push_back(&rd);
        return regs;
    }
};

struct PUSH : public Instr {
    std::pair<Reg, Reg> range;
    std::vector<Reg> regs;
    PUSH() : Instr(kPUSH) {}

    virtual std::string str() const {
        std::string s = "PUSH ";
        bool need_brace = regs.size() > 1;
        if (range.first != range.second) {
            need_brace = true;
            s += '{' + Reg2Str(range.first) + "-" + Reg2Str(range.second);
            if (regs.size() > 0)
                s += ", ";
        }
        bool is_first = true;
        for (auto r : regs) {
            if (!is_first)
                s += ",";
            s += Reg2Str(r);
            is_first = false;
        }
        if (need_brace)
            s += '}';
        return s;
    }

    virtual std::vector<Reg *> RRegs() {
        std::vector<Reg *> r;
        for (auto &reg : regs)
            r.push_back(&reg);
        return r;
    }
};

struct POP : public Instr {
    std::pair<Reg, Reg> range;
    std::vector<Reg> regs;
    POP() : Instr(kPOP) {}

    virtual std::string str() const {
        std::string s = "POP ";
        bool need_brace = regs.size() > 1;
        if (range.first != range.second) {
            need_brace = true;
            s += '{' + Reg2Str(range.first) + "-" + Reg2Str(range.second);
            if (regs.size() > 0)
                s += ", ";
        }
        bool is_first = true;
        for (auto r : regs) {
            if (!is_first)
                s += ",";
            s += Reg2Str(r);
            is_first = false;
        }
        if (need_brace)
            s += '}';
        return s;
    }

    virtual std::vector<Reg *> WRegs() {
        std::vector<Reg *> r;
        for (auto &reg : regs)
            r.push_back(&reg);
        return r;
    };
};

struct ADR : public Instr {
    Reg rd;
    std::string label;
    ADR() : Instr(kADR) {}

    virtual std::string str() const {
        return "ADR " + Reg2Str(rd) + ", " + label;
    }

    std::vector<Reg *> WRegs() { return make_regs(rd); }
};

struct Branch : public Instr {
    Cond cond;
    std::string label;
    Branch() : Instr(kB) {}
    Branch(Cond cond, std::string label)
        : Instr(kB), cond(cond), label(label) {}

    virtual std::string str() const { return "B" + cond.str() + " " + label; }
};

struct Call : public Instr {
    std::string func;
    std::vector<Reg> args;
    Reg ret;

    Call() : Instr(kCall) { ret = Reg::INVALID; }

    virtual std::string str() const {
        std::string s = "BL " + func + " // call ";
        for (auto x : args)
            s += Reg2Str(x) + " ";
        if (ret != Reg::INVALID)
            s += "-> " + Reg2Str(ret);
        return s;
    }

    virtual std::vector<Reg *> RRegs() {
        std::vector<Reg *> res;
        for (auto &r : args)
            res.push_back(&r);
        return res;
    }

    virtual std::vector<Reg *> WRegs() {
        if (ret != Reg::INVALID)
            return make_regs(ret);
        return {};
    };
};

struct BranchLink : public Instr {
    std::string label;
    BranchLink() : Instr(kBL) {}
    BranchLink(std::string label) : Instr(kBL), label(label) {}

    virtual std::string str() const { return "BL " + label; }
};

struct BinaryAlu : public Instr {
    Reg rd, a;
    RegImmU b;
    Shift shift;

    BinaryAlu(int op) : Instr(op) {}

    std::string OpStr() const {
        static std::string m[]{"ADD", "SUB", "MUL", "SDIV", "UDIV",
                               "AND", "OR",  "ADC", "LSL",  "LSR"};
        return m[op - kADD];
    }

    bool IsCommutative() {
        return op == kADD || op == kMUL || op == kAND || op == kOR;
    }

    virtual std::string str() const {
        std::string s = OpStr() + " " + Reg2Str(rd) + ", " + Reg2Str(a) + ", " +
                        RegImmU2Str(b, flags);
        if (flags & kFlagHasShift) {
            s += ", " + shift.str();
        }
        return s;
    }

    virtual std::vector<Reg *> RRegs() {
        std::vector<Reg *> res;

        res.push_back(&a);
        if (!(flags & kFlagBIsImm))
            res.push_back(&b.r);
        return res;
    }
    virtual std::vector<Reg *> WRegs() { return make_regs(rd); };
};

struct MOV : public Instr {
    Reg rd;
    RegImmU src;
    MOV() : Instr(kMOV) {}
    MOV(Reg rd, Reg r) : rd(rd) { src.r = r; }
    MOV(Reg rd, Word imm) : rd(rd) {
        flags |= kFlagBIsImm;
        src.imm = imm;
    }

    virtual std::string str() const {
        return "MOV " + Reg2Str(rd) + ", " + RegImmU2Str(src, flags);
    }

    virtual std::vector<Reg *> RRegs() {
        if (!(flags & kFlagBIsImm))
            return make_regs(src.r);
        return {};
    }
    virtual std::vector<Reg *> WRegs() { return make_regs(rd); };
};

struct NEG : public Instr {
    Reg rd, rs;
    NEG() : Instr(kNEG) {}

    virtual std::string str() const {
        return "NEG " + Reg2Str(rd) + ", " + Reg2Str(rs);
    }

    virtual std::vector<Reg *> RRegs() { return make_regs(rs); }
    virtual std::vector<Reg *> WRegs() { return make_regs(rd); };
};

struct CMP : public Instr {
    Reg a;
    RegImmU b;
    CMP() : Instr(kCMP) {}

    virtual std::string str() const {
        return "CMP " + Reg2Str(a) + ", " + RegImmU2Str(b, flags);
    }

    virtual std::vector<Reg *> RRegs() {
        std::vector<Reg *> res;

        res.push_back(&a);
        if (!(flags & kFlagBIsImm))
            res.push_back(&b.r);
        return res;
    }
};

struct LabelInstr : public Instr {
    std::string label;

    LabelInstr(std::string label) : label(label), Instr(kLabel) {}

    virtual std::string str() const { return label + ":"; }
};

struct BB {
    std::string label;
    std::list<std::unique_ptr<Instr>> insts;
    std::unique_ptr<Branch> branch;
    std::vector<BB *> succs;
    std::vector<BB *> preds;
    int id;

    ir::BB *ir_bb;

    void dump(std::ostream &os) {
        if (label.length() > 0)
            os << label << ":\n";
        for (auto &inst : insts) {
            os << "    " << inst->str() << "\n";
        }
        if (branch)
            os << "    " << branch->str() << "\n";
    }

    void SetBranch(Cond cond, std::string label) {
        branch = std::make_unique<Branch>(cond, label);
    }
};

struct StackFrame {
    // stack layout
    // sp ----
    //    call arguments reserve area
    //    ----
    //    backup r0-r4
    // r7 ----
    //    local variable area
    //    ---
    //    rx - r7
    //    lr
    // --- prev_sp ---
    //    function arguments spilled
    // ---

    int max_call_arg_count;
    int local_var_size;
    int saved_reg_count;
    int spilled_arg_count;

    const static int max_reg_arg_count = 4;
    const static Reg local_var_base = Reg::R7;

    StackFrame() {
        max_call_arg_count = 0;
        local_var_size = 0;
        saved_reg_count = 0;
        spilled_arg_count = 0;
    }

    void Dump(std::ostream &os) {
        os << "[sp, 0] -------------------------------\n"
           << "\t" << max_call_arg_count * 4
           << " (call arguments reserve area)\n"
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

    // sp +
    inline int LocalVarBaseOffset() {
        return (max_call_arg_count + max_reg_arg_count) * 4;
    }

    inline int AllocaSize() { return LocalVarBaseOffset() + local_var_size; }

    inline int Size() { return AllocaSize() + SaveRegSize(); }

    inline int SaveRegSize() { return 9 * 4; }

    // align to high address
    inline int AlignHigh(int offset, int alignment) {
        return offset;
        // if (offset == 0)
        //     return offset;
        // return ((offset - 1) / alignment + 1) * alignment;
    }

    int AllocaVar(int size, int alignment = 4) {
        int offset = AlignHigh(local_var_size, alignment);
        local_var_size = offset + size;
        return offset;
    }

    Address CallArgAddr(int idx) {
        assert(idx >= max_reg_arg_count);
        Address addr;
        addr.mode = Address::kMBaseImm;
        addr.base = Reg::SP;
        addr.offset.imm = (idx - max_reg_arg_count) * 4;
        return addr;
    }

    Address BackupAddress(Reg r) {
        assert((int)r < max_reg_arg_count);
        Address addr;
        addr.mode = Address::kMBaseImm;
        addr.base = Reg::SP;
        addr.offset.imm = (max_call_arg_count + (int)r) * 4;
        return addr;
    }

    Address VarAddr(int offset) {
        Address addr;
        addr.mode = Address::kMBaseImm;
        addr.base = local_var_base;
        addr.offset.imm = offset;
        return addr;
    }

    // r7 +
    inline int SpilledArgOffset() { return local_var_size + SaveRegSize(); }

    // r7 +
    int ArgOffset(int idx) {
        if (idx < max_reg_arg_count)
            return local_var_size + idx * 4;
        else
            return SpilledArgOffset() + (idx - max_reg_arg_count) * 4;
    }

    Address ArgAddr(int idx) {
        Address addr;
        addr.mode = Address::kMBaseImm;
        addr.base = local_var_base;
        addr.offset.imm = ArgOffset(idx);
        return addr;
    }
};

struct Func {
    std::string name;
    BB entry, end;
    std::vector<std::unique_ptr<BB>> bbs;
    std::map<Word, std::unique_ptr<Imm>> imms;
    Comment cmt;

    std::vector<Reg> args;
    bool has_ret;

    StackFrame frame;

    int vreg;

    Func() { vreg = (int)Reg::VREG; }
    Func(std::string name) : Func() {
        this->name = name;
        end.label = "__fend__" + name;
        vreg = (int)Reg::VREG;
    }

    void dump(std::ostream &os) {
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
        for (auto &[label, imm] : imms) {
            os << label << ":\n" << imm->str() << "\n";
        }
    }

    std::string CreateIntImm(Word v) {
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

    Reg AllocaVReg() { return (Reg)vreg++; }

    void ResetBBID() {
        int i = 0;
        for (auto &bb : bbs)
            bb->id = i++;
    }
};

struct Asm {
    std::vector<std::unique_ptr<Func>> funcs;

    void dump(std::ostream &os) {
        os << "\t.text\n"
              "\t.global\tmain\n"
              "\t.syntax unified\n"
              "\t.code\t16\n"
              "\t.thumb_func\n"
              "\t.fpu softvfp\n"
              "\t.type\tmain, %function\n";
        for (auto &f : funcs) {
            f->dump(os);
            os << "\n";
        }
    }
};

namespace builder {
using instr_list = std::list<std::unique_ptr<Instr>>;
using instr_iter = std::list<std::unique_ptr<Instr>>::iterator;
void Load(instr_list &insts, instr_iter it, Reg rd, Address addr);
void LAddr(instr_list &insts, instr_iter it, Reg rd, std::string label);
void Store(instr_list &insts, instr_iter it, Reg rd, Address addr);
void Push(instr_list &insts, instr_iter it, Reg r);
void Push(instr_list &insts, instr_iter it, Reg from, Reg to, Reg r);
void Pop(instr_list &insts, instr_iter it, Reg r);
void Pop(instr_list &insts, instr_iter it, Reg from, Reg to, Reg r);
void Branch(instr_list &insts, instr_iter it, std::string label);
void Branch(instr_list &insts, instr_iter it, Cond cond, std::string label);
void Call(instr_list &insts, instr_iter it, std::string func,
          std::vector<Reg> args, Reg ret = Reg::INVALID);
void BranchLink(instr_list &insts, instr_iter it, std::string func);
void BinaryAlu(instr_list &insts, instr_iter it, int op, Reg rd, Reg a, Reg b);
void BinaryAlu(instr_list &insts, instr_iter it, int op, Reg rd, Reg a, Word b);
void Move(instr_list &insts, instr_iter it, Reg rd, Reg rs);
void Move(instr_list &insts, instr_iter it, Reg rd, Word imm);
void Cmp(instr_list &insts, instr_iter it, Reg a, Reg b);
void Cmp(instr_list &insts, instr_iter it, Reg a, Word b);
void Label(instr_list &insts, instr_iter it, std::string label);

namespace adv {
// advanced builder, can handle immediate value automatically

struct Operand {
    Reg r;
    Word imm;
    int flags;
    enum { kIsImm = 1 };

    Operand(Reg r) : r(r) { flags = 0; }
    Operand(Word imm) : imm(imm) { flags = kIsImm; }
};

Reg Op2Reg(Func &func, instr_list &insts, instr_iter it, Operand op,
           Reg rd = Reg::INVALID);

void BinaryAlu(Func &func, instr_list &insts, instr_iter it, int op, Operand r,
               Operand a, Operand b, bool has_shift = false, Shift shift = {});
void Cmp(Func &func, instr_list &insts, instr_iter it, Operand a, Operand b);

void Move(Func &func, instr_list &insts, instr_iter it, Operand rd, Operand rs);

} // namespace adv

}; // namespace builder

void IrToAsm(ir::Module &m, Asm &_asm, bool disable_ra = false);

} // namespace backend

#endif
