#ifndef __backend_h__
#define __backend_h__

#include "ir/ir.hpp"
#include <string>
#include <vector>
#include <memory>
#include <list>
#include <queue>
#include <set>
#include <algorithm>

namespace backend {

// basic
/********************************************************************************************/
enum struct Reg {   // 寄存器定义
    R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11,
    FP = R11,
    R12, R13,
    SP,
    LR,
    PC,
    VREG,   // 临时寄存器
    INVALID = -1,
};
std::string Reg2Str(Reg r);  
typedef unsigned Word;

struct Cond {    // 条件执行
    int type;
    enum { EQ, NE, HI, HS, LS, LO, GT, GE, LT, LE, AL };
    Cond() : type(AL) {}    // always execute
    Cond(int type) : type(type) {}

    std::string str() const;
    Cond Not();
};

struct Shift {     // 桶形移位器
    int type;
    int imm;
    enum { kLSL };

    std::string str() const;
};

struct Address {      // 地址偏移
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
    int mode;           // 偏移模式
    Reg base;           // 标准寄存器
    std::string label;  // 标签型偏移 全局数据

    const static int mode_index_mask = 0b11;
    const static int mode_m_mask = 0b11100;
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
    struct {     // 偏移元
        unsigned imm;
        Reg reg;
        Shift shift;
    } offset;
    

    Address() {}
    Address(int mode) : mode(mode) {}
    Address(Reg base) {
        mode = kMBase;
        this->base = base;
    }
    Address(std::string label) {
        mode = KMLabel;
        std::string eq = "=";
        this->label = eq + label;
    }

    std::string str() const;
    std::vector<Reg *> RRegs();   // 提取 偏移使用的寄存器
};

typedef union {
    Reg r;
    Word imm;
} RegImmU;

std::string RegImmU2Str(RegImmU x, int flags);


struct Define
{
    bool is_const;
    std::string name;
    std::unique_ptr<ir::InitVal> init;

    Define() { is_const = false;}
    std::string str();
};

/********************************************************************************************/



// Inst
/********************************************************************************************/
struct Line {
    virtual std::string str() const = 0;
};
struct Comment : public Line {   // 注释
    std::string data;
    operator bool() const { return data.length() > 0; }
    virtual std::string str() const { return data; };
};

struct Imm : public Line {      // 立即数  名字 类型  值
    std::string label;
    std::string ty;
    Word v;
    virtual std::string str() const {
        return label + ":\n" + ty + " " + std::to_string(v);
    }
};

struct ImmArray : public Line {   // 立即数块
    std::string label;
    std::string ty;
    std::vector<Word> vals;
    virtual std::string str() const {
        std::string s = label + ":\n" + ty + " ";
        for (auto v : vals)
            s += std::to_string(v) + " ";
        return s;
    }
};

struct Instr : public Line {
    int op;
    int flags;
    Comment cmt;

    enum {    // 列举指令集
        kLDR, kSTR, kPUSH, kPOP, kADR,
        kB, kBL,
        kADD, kSUB, kMUL, kSDIV, kUDIV, kAND, kOR, kADC, kLSL, kLSR,
        kMOV, kNEG,
        kCMP,
        kCall,kLabel,
    };

    enum {    // 指令标识
        kFlagS = 1,
        kFlagBIsImm = 2,
        kFlagHasShift = 4,
    };
    

    Instr() { flags = 0; }
    Instr(int op) : op(op) { flags = 0; }
    virtual std::string str() const = 0;
    virtual std::vector<Reg *> RRegs() { return {}; }
    virtual std::vector<Reg *> WRegs() { return {}; };

    std::vector<Reg *> Regs(){   // 合并寄存器堆(W -> R)
        auto res = RRegs();
        for (auto r : WRegs())
            res.push_back(r);
        return res;
    }

  protected:
    // 将一个寄存器转化为寄存器堆形式
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
        return addr.RRegs();
    }
    virtual std::vector<Reg *> WRegs() {
        return make_regs(rd);
    }
};

struct Branch : public Instr {
    Cond cond;
    std::string label;
    Branch() : Instr(kB) {}
    Branch(Cond cond, std::string label)
        : Instr(kB), cond(cond), label(label) {}

    virtual std::string str() const { return "B" + cond.str() + " " + label; }
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


/********************************************************************************************/




// Func
/********************************************************************************************/
struct BB {
    std::string label;                          // 基本块标识
    std::list<std::unique_ptr<Instr>> insts;    // 指令链
    std::unique_ptr<Branch> branch;             // 跳转分支
    std::vector<BB *> succs;                    // 后继
    std::vector<BB *> preds;                    // 前驱
    int id;                                     // 基本块编号
    ir::BB *ir_bb;                              // 对应的IR基本块

    void dump(std::ostream &os);
    void SetBranch(Cond cond, std::string label);  // 根据条件标识更新跳转分支
};

struct StackFrame {   // to do
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

    int max_call_arg_count;     // 传递参数个数
    int local_var_size;         // 局部变量个数
    int saved_reg_count;        // 存储
    int spilled_arg_count;      // 溢出

    const static int max_reg_arg_count = 4;     // 比赛最大传参 4
    const static Reg local_var_base = Reg::R7;  // 

    StackFrame() {
        max_call_arg_count = 0;
        local_var_size = 0;
        saved_reg_count = 0;
        spilled_arg_count = 0;
    }

    void Dump(std::ostream &os) ;

    // sp +
    inline int LocalVarBaseOffset() { return (max_call_arg_count + max_reg_arg_count) * 4; }
    inline int AllocaSize() { return LocalVarBaseOffset() + local_var_size; }
    inline int Size() { return AllocaSize() + SaveRegSize(); }
    inline int SaveRegSize() { return 9 * 4; }

    // align to high address
    inline int AlignHigh(int offset, int alignment);
    int AllocaVar(int size, int alignment = 4);
    Address CallArgAddr(int idx);
    Address BackupAddress(Reg r);
    Address VarAddr(int offset);

    // r7 +
    inline int SpilledArgOffset();

    // r7 +
    int ArgOffset(int idx);
    Address ArgAddr(int idx);
};

struct Func {
    std::string name;                           // 函数名
    BB entry, end;                              // 函数入口，出口
    std::vector<std::unique_ptr<BB>> bbs;       // 函数基本块
    std::map<Word, std::unique_ptr<Imm>> imms;  // 函数常量
    Comment cmt;                                // 注释
    std::vector<Reg> args;                      // 参数
    bool has_ret;                               // 返回值标识
    StackFrame frame;                           // 栈帧
    int vreg;                                   // 临时寄存器数量

    Func() { vreg = (int)Reg::VREG; }
    Func(std::string name) : Func() {
        this->name = name;
        end.label = "__fend__" + name;
        vreg = (int)Reg::VREG;
    }

    void dump(std::ostream &os) ;
    std::string CreateIntImm(Word v);   // 创建常量
    Reg AllocaVReg() ;                  // 分配一个新的寄存器
    void ResetBBID();                   // 为标准块重新编号
};
/********************************************************************************************/



// builder
/********************************************************************************************/
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
/********************************************************************************************/




// Asm
/********************************************************************************************/
struct Asm {  // 由若干函数组成
    
    std::vector<std::unique_ptr<Define>> defs;
    std::vector<std::unique_ptr<Func>> funcs;

    void dump(std::ostream &os);
};


const std::string DumpVregCode = "dump-vreg-code";
const std::string DbgConvertCall = "convert-call";
const std::string DbgConvertPhi = "convert-phi";


struct InstrSelectHelper {    // 选择指令，完成转化
    ir::Module &m;                              // LLVM IR
    Asm &_asm;                                  // ARM
    Func *func;                                 // 当前函数对象
    Define *def;                                // 当前全局声明
    BB *bb;                                     // 当前基本块
    std::map<ir::Value *, Reg> v_to_vreg;       // 变量与寄存器映射表

    InstrSelectHelper(ir::Module &m, Asm &_asm) : m(m), _asm(_asm) {
        VRegReset();
    }

    void VRegReset();                                                 
    Reg GetVReg(std::shared_ptr<ir::Value> v = nullptr);
    int IrAluOpToAsmOp(int op);

    Reg GetGlobalAdrr(std::shared_ptr<ir::Value> g);
    Reg LoadGlobal(Reg rt, std::shared_ptr<ir::Value> g);
    void StoreGlobal(std::shared_ptr<ir::Value> g, Reg rd);

    builder::adv::Operand GetOperand(std::shared_ptr<ir::Value> v);
    builder::adv::Operand GetOperand(Reg r);
    builder::adv::Operand GetOperand(Word imm);
    void ConvertBinaryAlu(ir::BinaryAlu *alu);
    void ConvertOtherInst(ir::Instr *inst);
    void ConvertGetelementPtr(ir::Getelementptr *getelementptr);
    int ShiftCount(Word x);
    Cond IrCond2AsmCond(int cond);
    Reg ConvertIcmpI32(ir::Icmp *icmp);
    std::string GetBBLabel(std::shared_ptr<ir::LocalValue> l);

    void Build();
};


namespace regalloca {

const std::string DbgRegAlloca = "reg-alloca";
const std::string DbgLiveAnalysis = "live-analysis";
const std::string DbgDisableBuiltBackup = "dis-builtin-bck";

struct Location {   // 活跃点
    int bb;
    int n;

    Location() { bb = 0, n = 0; }
    Location(int bb_, int n_) : bb(bb_), n(n_){}

    inline Location inc(int N) const {
        Location res = *this;
        if (n + 1 == N)
            res.bb++, res.n = 0;
        else
            res.n++;
        return res;
    }

    inline Location dec(int prevN) const {
        Location res = *this;
        if (n - 1 < 0)
            res.bb--, res.n = prevN - 1;
        else
            res.n--;
        return res;
    }

    friend bool operator<(const Location &a, const Location &b) {
        if(a.bb < b.bb)  return 1;
        if(a.bb == b.bb)
        {
            if(a.n != -1 && a.n < b.n)  return 1;
            if(a.n != -1 && b.n == -1)  return 1;  
        }
        return 0;
    }

    friend bool operator==(const Location &a, const Location &b) {
        return a.bb == b.bb && a.n == b.n;
    }

    friend bool operator>(const Location &a, const Location &b) {
        if(a.bb > b.bb)  return 1;
        if(a.bb == b.bb)
        {
            if(a.n == -1 && b.n != -1)  return 1;
            if(a.n > b.n && b.n != -1)  return 1;  
        }
        return 0;
    }
};

struct Range {   
    Location start, end;

    inline bool contain(const Location &loc) {
        return !(loc < start) && !(loc > end);
    }
};

struct LiveInterval {
    Reg vreg;   // 虚拟寄存器编号
    Range lr;   // 活跃区间

    int state;
    enum {
        kVReg,
        kPhyReg,
        kStack,
    };
    Reg phy_reg;    // 实际寄存器编号
    int stack_slot;
    bool stack_allocated;   // 需要在栈上分配空间

    LiveInterval() {
        state = kVReg;
        stack_allocated = false;
    }

    bool CheckInterfering(const Range &rng);
    void AssignPhyReg(Reg r);
    void AssignStackSlot(int slot) ;
    bool Assigned() const;
    Address GetAddr(StackFrame &frame);

    void Dump(std::ostream &os, StackFrame *frame = nullptr);
};

struct RegAllocaHelper {
    struct CmpIntervalStartNLess {
        bool operator()(const std::shared_ptr<LiveInterval> &a,
                        const std::shared_ptr<LiveInterval> &b) const {
            return !(a->lr.start < b->lr.start);
        }
    };

    Func &func;
    const int phy_reg_count = 7;    // Thumb能够使用的寄存器 7 个
    const int max_reg_arg_count = 4;   // Sys2022传递参数最多4个

    std::priority_queue <std::shared_ptr<LiveInterval>,
                        std::vector<std::shared_ptr<LiveInterval>>,
                        CmpIntervalStartNLess>
        intervals;            // 用于排列区间
    std::vector<std::shared_ptr<LiveInterval>> phy_intervals;
    std::vector<std::shared_ptr<LiveInterval>> active;
    // // pre-alloca vreg
    // std::map<Reg, std::shared_ptr<LiveInterval>> pre_alloca;
    // current alloca status: verg -> interval
    std::map<Reg, std::shared_ptr<LiveInterval>> alloca_map;
    // free registers at loaction
    std::map<Location, std::vector<Reg>> loc_free_regs;

    // max location, set by CollectLiveInfo
    Location max_loc;  // IR指令标号上限
    // max n of each bb
    std::vector<int> bb_loc_N;

    // defs 已定义的指令
    std::vector<std::set<Reg>> defs, uses, livein, liveout;

    RegAllocaHelper(Func &f) : func(f) {
        phy_intervals.resize(phy_reg_count);
        defs.resize(f.bbs.size());
        uses.resize(f.bbs.size());
        livein.resize(f.bbs.size());
        liveout.resize(f.bbs.size());
        f.ResetBBID();
    }

    std::shared_ptr<LiveInterval> Dequeue();

    void Enqueue(std::shared_ptr<LiveInterval> v);
    void CollectLiveInfo();
    void DumpLiveAnalysis();
    void Spill(std::shared_ptr<LiveInterval> &interval);
    void AssignPhyReg(std::shared_ptr<LiveInterval> interval);
    void DumpIntervals() ;
    void DumpAllocaMap();
    std::shared_ptr<LiveInterval> PopLastLongestActive();

    void Alloca();
    bool FuncIsBuiltIn(std::string name);
    bool GetBackupRegs(std::vector<Reg> &regs, Reg ret, int count);
    void ReWrite();
};

} // namespace regalloca


/********************************************************************************************/

// emit_asm  = 1  不输出  “0 size : ty ....” 部分
void IrToAsm(ir::Module &m, Asm &_asm, bool disable_ra = false, bool emit_asm = true);
void InstrSelect(ir::Module &m, Asm &_asm);
void RegAlloca(Asm &_asm);   ///

}   // namespace backend


#endif