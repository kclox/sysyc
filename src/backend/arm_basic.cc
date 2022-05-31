#include "backend.h"
#include <stdarg.h>


namespace backend {

std::string Reg2Str(Reg r)   {
    static std::string m[]{
        "R0", "R1",  "R2", "R3",  "R4",  "R5", "R6", "R7", "R8",
        "R9", "R10", "FP", "R12", "R13", "SP", "LR", "PC",
    };
    if (r == Reg::INVALID)
        return "INVALID";
    if ((unsigned)r < (unsigned)Reg::VREG)
        return m[(unsigned)r];
    else
        return "V" + std::to_string((unsigned)r - (unsigned)Reg::VREG);
}

std::string RegImmU2Str(RegImmU x, int flags) {
    if (flags & Instr::kFlagBIsImm)
        return "#" + std::to_string(x.imm);
    return Reg2Str(x.r);
}

// Cond
std::string Cond::str() const {
    static std::string m[]{"EQ", "NE", "HI", "HS", "LS", "LO",
                               "GT", "GE", "LT", "LE", ""};
    return m[type];
}
Cond Cond::Not() {
    static std::map<int, int> m{
            {EQ, NE}, {NE, EQ}, {HI, LS}, {HS, LO}, {LO, HS},
            {LS, HI}, {GT, LE}, {GE, LT}, {LT, GE}, {LE, GT},
    };
    return Cond(m[type]);
}

// Shift
std::string Shift :: str() const {
    static std::string m[]{"LSL"};
    return m[type] + " #" + std::to_string(imm);
}


// Address
std::string Address::str() const {
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
std::vector<Reg *> Address::RRegs() {
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

// Define
std::string Define::str()
{
    std::string s = name + ":\n";
    s += init->str_arm();
    return s;
}


};
