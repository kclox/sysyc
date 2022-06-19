#include "backend.h"
#include "dbg.hpp"
#include <algorithm>
#include <queue>
#include <set>

namespace backend {

namespace regalloca {

#define BACK(v) v, (v).end()

const std::string DbgRegAlloca = "reg-alloca";
const std::string DbgLiveAnalysis = "live-analysis";
const std::string DbgDisableBuiltBackup = "dis-builtin-bck";

struct Location { // 活跃点
    int bb;
    int n;

    Location() { bb = 0, n = 0; }
    Location(int bb_, int n_) : bb(bb_), n(n_) {}

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
        if (a.bb < b.bb)
            return 1;
        if (a.bb == b.bb) {
            if (a.n != -1 && a.n < b.n)
                return 1;
            if (a.n != -1 && b.n == -1)
                return 1;
        }
        return 0;
    }

    friend bool operator==(const Location &a, const Location &b) {
        return a.bb == b.bb && a.n == b.n;
    }

    friend bool operator>(const Location &a, const Location &b) {
        if (a.bb > b.bb)
            return 1;
        if (a.bb == b.bb) {
            if (a.n == -1 && b.n != -1)
                return 1;
            if (a.n > b.n && b.n != -1)
                return 1;
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
    Reg vreg; // 虚拟寄存器编号
    Range lr; // 活跃区间

    int state;
    enum {
        kVReg,
        kPhyReg,
        kStack,
    };
    Reg phy_reg; // 实际寄存器编号
    int stack_slot;
    bool stack_allocated; // 需要在栈上分配空间

    LiveInterval() {
        state = kVReg;
        stack_allocated = false;
    }

    bool CheckInterfering(const Range &rng);
    void AssignPhyReg(Reg r);
    void AssignStackSlot(int slot);
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
    const int phy_reg_count = 7;     // Thumb能够使用的寄存器 7 个
    const int max_reg_arg_count = 4; // Sys2022传递参数最多4个

    std::priority_queue<std::shared_ptr<LiveInterval>,
                        std::vector<std::shared_ptr<LiveInterval>>,
                        CmpIntervalStartNLess>
        intervals; // 用于排列区间
    std::vector<std::shared_ptr<LiveInterval>> phy_intervals;
    std::vector<std::shared_ptr<LiveInterval>> active;
    // // pre-alloca vreg
    // std::map<Reg, std::shared_ptr<LiveInterval>> pre_alloca;
    // current alloca status: verg -> interval
    std::map<Reg, std::shared_ptr<LiveInterval>> alloca_map;
    // free registers at loaction
    std::map<Location, std::vector<Reg>> loc_free_regs;

    // max location, set by CollectLiveInfo
    Location max_loc; // IR指令标号上限
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
    void DumpIntervals();
    void DumpAllocaMap();
    std::shared_ptr<LiveInterval> PopLastLongestActive();

    void Alloca();
    bool FuncIsBuiltIn(std::string name);
    bool GetBackupRegs(std::vector<Reg> &regs, Reg ret, int count);
    void ReWrite();
};

bool LiveInterval::CheckInterfering(const Range &rng) {
    return lr.contain(rng.start) || lr.contain(rng.end);
}

void LiveInterval::AssignPhyReg(Reg r) {
    phy_reg = r;
    state = kPhyReg;
}

void LiveInterval::AssignStackSlot(int slot) {
    stack_slot = slot;
    state = kStack;
}

bool LiveInterval::Assigned() const { return state != kVReg; }
Address LiveInterval::GetAddr(StackFrame &frame) {
    assert(state == kStack);
    return frame.VarAddr(stack_slot);
}
void LiveInterval::Dump(std::ostream &os, StackFrame *frame) {
#define DUMP_LOC(loc) "(" << loc.bb << ", " << loc.n << ")"
    os << Reg2Str(vreg) << "\t" << DUMP_LOC(lr.start) << " -> "
       << DUMP_LOC(lr.end);
    if (Assigned())
        if (state == kPhyReg)
            os << " " << Reg2Str(phy_reg);
        else {
            if (frame)
                os << " " << GetAddr(*frame).str();
            else
                os << " " << stack_slot;
        }
    std::cerr << std::endl;
}

std::shared_ptr<LiveInterval> RegAllocaHelper::Dequeue() {
    if (intervals.size() == 0)
        return {};
    auto v = intervals.top();
    intervals.pop();
    return v;
}

void RegAllocaHelper::Enqueue(std::shared_ptr<LiveInterval> v) {
    intervals.push(v);
}

void RegAllocaHelper::CollectLiveInfo() { // 活跃信息
    std::map<Reg, std::shared_ptr<LiveInterval>> vreg_to_interval;
    Location &loc = max_loc;

    int i = 0;
    for (auto vr : func.args) {
        defs[0].insert(vr);
        auto interval = std::make_shared<LiveInterval>();
        interval->vreg = vr;
        interval->stack_allocated = true;
        interval->lr = {loc, loc};
        if (i >= max_reg_arg_count) { // 超过了参数上限，将其存入栈中
            interval->state = LiveInterval::kStack; // 栈
            alloca_map[vr] = interval;
        }
        vreg_to_interval[vr] = interval;
        Enqueue(interval); // 装入优先队列
        i++;
    }

    for (int i = 0; i < func.bbs.size(); i++) { // 提取出每个基本块
        auto &bb = func.bbs[i];
        loc.n = 0; // 对于一个新的块，行数置0
        for (auto &inst : bb->insts) {
            auto r_regs = inst->RRegs(); // 得到每条指令所用的  源寄存器
            for (Reg *r : r_regs) {
                if (*r < Reg::VREG) // 虚拟寄存器与实际对应，无需分配
                    continue;

                if (!vreg_to_interval[*r]) // 当前寄存器活跃区间不存在实例
                    continue;

                vreg_to_interval[*r]->lr.end = loc;
                uses[loc.bb].insert(*r);
            }

            auto w_regs = inst->WRegs();
            for (Reg *r : w_regs) {
                if (*r < Reg::VREG)
                    continue;

                defs[loc.bb].insert(*r);
                auto temp = vreg_to_interval.insert(
                    {*r, std::make_shared<LiveInterval>()});
                if (temp.second) {
                    temp.first->second->vreg = *r;
                    temp.first->second->lr = {loc, loc};
                    Enqueue(temp.first->second);
                } else {
                    temp.first->second->lr.end = loc;
                }
            }
            loc.n++;
        }
        bb_loc_N.push_back(loc.n);
        loc.bb++;
    }
    max_loc.bb--, max_loc.n--;

    // bool changed = true;
    // while (changed) {
    //     changed = false;
    //     for (int id = 0; id < func.bbs.size(); id++) {
    //         std::set<Reg> nliveout;
    //         for (auto succ : func.bbs[id]->succs) {
    //             std::set<Reg> tmp = livein[succ->id];
    //             nliveout.merge(tmp);
    //         }
    //         if (nliveout != liveout[id]) {
    //             changed = true;
    //             liveout[id] = nliveout;
    //         }

    //         if (uses[id] != livein[id]) {
    //             livein[id] = uses[id];
    //             changed = true;
    //         }
    //     }
    //     if (DbgEnabled(DbgLiveAnalysis)) {
    //         DumpLiveAnalysis();
    //     }
    // }

    // update live range
    /*std::set<int> loop_header_bb;
    for (int i = 0; i < func.bbs.size(); i++) {
        auto &bb = func.bbs[i];
        for (auto pred : bb->preds) {
            if (pred->id > bb->id) {
                loop_header_bb.insert(i);
            }
        }
    }

    // 处理调用关系，根据调用图更新每个寄存器活跃区间
    for (auto i : loop_header_bb) {
        int max_id = 0;
        for (auto &succ : func.bbs[i]->succs)
            max_id = std::max(max_id, succ->id);
        for (auto vr : uses[i]) {
            auto &interval = vreg_to_interval[vr];
            if (DbgEnabled(DbgLiveAnalysis)) {
                fprintf(stderr, "update lr %s after liveness analysis\n",
                        Reg2Str(vr).c_str());
            }
            if(interval->lr.end.bb <= max_id)
            {
                interval->lr.end.bb = max_id;
                interval->lr.end.n = -1;
            }

        }
    }*/

    // 采用最贪心的方法解决活跃区间
    Location L, R;
    std::set<std::shared_ptr<LiveInterval>> buffer;
    std::shared_ptr<LiveInterval> interval;
    for (int i = 0; i < func.bbs.size(); i++) {
        auto &bb = func.bbs[i];
        for (auto succs : bb->succs) {
            if (succs->id < bb->id) {
                buffer.clear();
                R = {bb->id, -1};
                L = {succs->id, 0};
                while (intervals.size() > 0) {
                    interval = Dequeue();
                    if (interval->lr.start < L && interval->lr.end > L)
                        if (R > interval->lr.end)
                            interval->lr.end = R;
                    buffer.insert(interval);
                    if (interval->lr.start > L)
                        break;
                }
                for (auto k : buffer)
                    Enqueue(k);
            }
        }
    }
    // for (auto vr : liveout[i]) {
    //     auto &interval = vreg_to_interval[vr];
    //     if (interval->lr.end.bb < i) {
    //         if (DbgEnabled(DbgLiveAnalysis)) {
    //             fprintf(stderr,
    //                     "update lr %s after liveness analysis\n",
    //                     Reg2Str(vr).c_str());
    //         }
    //         interval->lr.end.bb = i;
    //         interval->lr.end.n = bb_loc_N[i] - 1;
    //     }
    // }
}

void RegAllocaHelper::DumpLiveAnalysis() {
    fprintf(stderr, "------ dump live analysis ------\n");
    for (int i = 0; i < func.bbs.size(); i++) {
        fprintf(stderr, "bb %d\n", i);
        fprintf(stderr, "  livein: ");
        for (auto x : livein[i]) {
            fprintf(stderr, "%s ", Reg2Str(x).c_str());
        }
        fprintf(stderr, "\n  liveout: ");
        for (auto x : liveout[i]) {
            fprintf(stderr, "%s ", Reg2Str(x).c_str());
        }
        fprintf(stderr, "\n");
    }
}

void RegAllocaHelper::Spill(std::shared_ptr<LiveInterval> &interval) {
    if (!interval->stack_allocated)
        interval->stack_slot = func.frame.AllocaVar(4);
    interval->state = LiveInterval::kStack;
    alloca_map[interval->vreg] = interval;
}

void RegAllocaHelper::AssignPhyReg(std::shared_ptr<LiveInterval> interval) {
    bool found = false;
    for (int i = 0; i < phy_reg_count; i++) {
        if (!phy_intervals[i]) {
            interval->phy_reg = (Reg)i;
            interval->state = LiveInterval::kPhyReg;
            phy_intervals[i] = interval;
            alloca_map[interval->vreg] = interval;
            found = true;
            break;
        }
    }
    assert(found);
}

void RegAllocaHelper::DumpIntervals() {
    std::cerr << "--- DumpIntervals ---\n";
    auto dup_intervals = intervals;
    for (std::shared_ptr<LiveInterval> interval; !dup_intervals.empty();) {
        interval = dup_intervals.top();
        dup_intervals.pop();
        interval->Dump(std::cerr);
    }
}

void RegAllocaHelper::DumpAllocaMap() {
    std::cerr << "--- DumpAllocaMap ---\n";
    for (auto i : alloca_map)
        i.second->Dump(std::cerr, &func.frame);
}

std::shared_ptr<LiveInterval> RegAllocaHelper::PopLastLongestActive() {
    auto max_it = active.begin();
    for (auto it = active.begin(); it != active.end(); it++) {
        if ((*it)->lr.end > (*max_it)->lr.end)
            max_it = it;
    }
    auto interval = *max_it;
    active.erase(max_it);
    return interval;
}

void RegAllocaHelper::Alloca() {
    CollectLiveInfo();

    if (DbgEnabled(DbgRegAlloca))
        DumpIntervals();

    Location loc;
    for (auto &bb : func.bbs) {
        loc.n = 0;
        for (auto &inst : bb->insts) { // 用于迭代loc
            // expired old interval
            // 将失活的interval所代表的寄存器清除
            for (auto it = active.begin(); it != active.end(); it++) {
                auto may_expired = *it;
                if (may_expired->lr.end < loc) {
                    if ((*it)->state == LiveInterval::kPhyReg)
                        phy_intervals[(int)(*it)->phy_reg] = nullptr;
                    active.erase(it--);
                }
            }

            // for each interval, whose lr start from loc
            for (std::shared_ptr<LiveInterval> interval;
                 intervals.size() > 0;) {
                interval = Dequeue();
                if (interval->lr.start > loc) {
                    Enqueue(interval);
                    break;
                }

                if (!interval->Assigned()) { // 为虚拟寄存器
                    active.push_back(interval);

                    // 寄存器堆中存在可用寄存器
                    if (active.size() < phy_reg_count) {
                        AssignPhyReg(interval);
                    } else {
                        // 取出最优寄存器
                        auto to_spill = PopLastLongestActive();
                        if (to_spill != interval) {
                            phy_intervals[(int)to_spill->phy_reg] = nullptr;
                            AssignPhyReg(interval);
                        }
                        Spill(to_spill); // 入栈
                    }
                }
            }

            // check stack slot reference count
            int stack_slot_refc = 0;
            if (inst->op != Instr::kCall)
                for (Reg *r : inst->Regs()) {
                    if (*r < Reg::VREG)
                        continue;
                    if (alloca_map[*r] &&
                        alloca_map[*r]->state == LiveInterval::kStack) {
                        stack_slot_refc++;
                    }
                }
            // reserve enough free registers for stack slot reference
            int spill_count = active.size() + stack_slot_refc - phy_reg_count;
            if (inst->op == Instr::kSTR)
                spill_count++;
            while (spill_count-- > 0) {
                auto to_spill = PopLastLongestActive();
                Spill(to_spill);
                phy_intervals[(int)to_spill->phy_reg] = nullptr;
            }

            // collection free registers at the location
            for (int i = 0; i < phy_reg_count; i++) {
                if (!phy_intervals[i])
                    loc_free_regs[loc].push_back((Reg)i);
            }
            loc.n++;
        }
        loc.bb++;
    }

    if (DbgEnabled(DbgRegAlloca)) {
        std::cerr << "before re-write\n";
        DumpAllocaMap();
        func.frame.Dump(std::cerr);
    }

    ReWrite();

    if (DbgEnabled(DbgRegAlloca)) {
        std::cerr << "after re-write\n";
        DumpAllocaMap();
        func.frame.Dump(std::cerr);
    }
}

bool RegAllocaHelper::FuncIsBuiltIn(std::string name) {
    static std::set<std::string> builtin_set{
        "getint",          "getch",          "getarray",
        "putint",          "putch",          "putarray",
        "starttime",       "stoptime",       "set_i32_array",
        "_sysy_starttime", "_sysy_stoptime", "_sysy_set_i32_array"};
    return builtin_set.count(name) > 0;
}

bool RegAllocaHelper::GetBackupRegs(std::vector<Reg> &regs, Reg ret,
                                    int count) {
    // has ret: r0 + args except ret_phy
    // no ret:  args except ret_phy
    Reg except = Reg::INVALID;
    if (ret != Reg::INVALID) {
        auto &interval = alloca_map[ret];
        if (interval && interval->state == LiveInterval::kPhyReg)
            except = interval->phy_reg;
        count = count == 0 ? 1 : count;
    }
    for (int i = 0; i < count; i++) {
        if ((Reg)i != except)
            regs.push_back((Reg)i);
    }
    return regs.size() > 0;
}

Word legal(Word x) {
    Word y;
    for (int i = 0; i < 32; i = i + 2) {
        y = (x >> (32 - i)) | (x << i);
        if (y < 256)
            return 1;
    }
    return 0;
}

void RegAllocaHelper::ReWrite() { // 初始化栈帧，采用满递减堆栈
    int x;
    // push r0-r7, lr
    int reg_arg_count = std::min((int)func.args.size(), max_reg_arg_count);
    int rx = std::max((int)func.has_ret, reg_arg_count);
    func.frame.saved_reg_count = 7 - rx + 2;
    builder::Push(BACK(func.entry.insts), Reg::R0, Reg::R7, Reg::LR);
    // sub sp, sp, x
    x = func.frame.AllocaSize();
    if (legal(x))
        builder::BinaryAlu(BACK(func.entry.insts), Instr::kSUB, Reg::SP,
                           Reg::SP, x);
    else {
        Address *imm = new Address(std::to_string(x));
        builder::Load(BACK(func.entry.insts), Reg::R7, *imm, true);
        builder::BinaryAlu(BACK(func.entry.insts), Instr::kSUB, Reg::SP,
                           Reg::SP, Reg::R7);
    }

    // add r7, sp, x
    x = func.frame.LocalVarBaseOffset();
    if (legal(x))
        builder::BinaryAlu(BACK(func.entry.insts), Instr::kADD, Reg::R7,
                           Reg::SP, x);
    else {
        Address *imm = new Address(std::to_string(x));
        builder::Load(BACK(func.entry.insts), Reg::R7, *imm, true);
        builder::BinaryAlu(BACK(func.entry.insts), Instr::kADD, Reg::R7,
                           Reg::SP, Reg::R7);
    }

    // move arg to allocated
    for (int i = 0; i < reg_arg_count; i++) {
        builder::Store(BACK(func.entry.insts), (Reg)i, func.frame.ArgAddr(i));
    }
    for (int i = 0; i < reg_arg_count; i++) {
        auto &interval = alloca_map[func.args[i]];
        if (interval->state == LiveInterval::kPhyReg)
            builder::Load(BACK(func.entry.insts), interval->phy_reg,
                          func.frame.ArgAddr(i));
        else
            interval->stack_slot = func.frame.ArgOffset(i);
    }
    for (int i = max_reg_arg_count; i < func.args.size(); i++) {
        auto &interval = alloca_map[func.args[i]];
        interval->stack_slot = func.frame.ArgOffset(i);
    }

    // re-write virtual register reference
    Location loc;
    for (auto &bb : func.bbs) {
        loc.n = 0;
        for (auto it_inst = bb->insts.begin(); it_inst != bb->insts.end();
             it_inst++) {
            auto inst = it_inst->get();

            // 将虚拟寄存器 转化为实际寄存器（函数）
            // load
            auto refer_vreg = [&](Reg vr, Reg rd = Reg::INVALID) -> Reg {
                auto &interval = alloca_map[vr];
                if (interval->state == LiveInterval::kPhyReg) {
                    return interval->phy_reg;
                } else {
                    if (rd == Reg::INVALID) {
                        rd = loc_free_regs[loc].back();
                        loc_free_regs[loc].pop_back();
                    }
                    builder::Load(bb->insts, it_inst, rd,
                                  interval->GetAddr(func.frame));
                }
                return rd;
            };
            // store
            auto write_vreg = [&](Reg vr, Reg rd = Reg::INVALID) -> Reg {
                auto &interval = alloca_map[vr];
                if (interval->state == LiveInterval::kPhyReg) {
                    return interval->phy_reg;
                } else {
                    if (rd == Reg::INVALID) {
                        rd = loc_free_regs[loc].back();
                        loc_free_regs[loc].pop_back();
                    }
                    builder::Store(bb->insts, it_inst, rd,
                                   interval->GetAddr(func.frame));
                }
                return rd;
            };

            if (inst->op == Instr::kCall) { // call
                auto call = dynamic_cast<Call *>(inst);
                int reg_arg_count =
                    std::min((int)call->args.size(), max_reg_arg_count);
                std::vector<Reg> backup_regs;
                int count = reg_arg_count;
                if (!DbgEnabled(DbgDisableBuiltBackup))
                    count = FuncIsBuiltIn(call->func) ? 4 : reg_arg_count;
                bool need_backup = GetBackupRegs(backup_regs, call->ret, count);
                if (need_backup) {
                    for (auto r : backup_regs) {
                        Address addr = func.frame.BackupAddress(r);
                        builder::Store(bb->insts, it_inst, r, addr);
                    }
                }

                bool has_ret = call->ret != Reg::INVALID;
                Reg reg_free = Reg::INVALID;
                if (backup_regs.size() > 0)
                    reg_free = backup_regs.front();

                // set a4, a5, ...
                for (int i = max_reg_arg_count; i < call->args.size(); i++) {
                    Reg rd = reg_free;
                    auto &interval = alloca_map[call->args[i]];
                    if (interval->state == LiveInterval::kPhyReg &&
                        interval->phy_reg == rd)
                        builder::Load(bb->insts, it_inst, rd,
                                      func.frame.BackupAddress(rd));
                    else
                        rd = refer_vreg(call->args[i], rd);
                    builder::Store(bb->insts, it_inst, rd,
                                   func.frame.CallArgAddr(i));
                }

                // set r0-r3
                for (int i = 0; i < reg_arg_count; i++) {
                    auto &interval = alloca_map[call->args[i]];
                    if (interval->state == LiveInterval::kPhyReg) {
                        // backup[x] -> rx
                        if (std::find(backup_regs.begin(), backup_regs.end(),
                                      interval->phy_reg) != backup_regs.end())
                            builder::Load(
                                bb->insts, it_inst, (Reg)i,
                                func.frame.BackupAddress(interval->phy_reg));
                        // phy -> rx
                        else if ((Reg)i != interval->phy_reg)
                            builder::Move(bb->insts, it_inst, (Reg)i,
                                          interval->phy_reg);
                    } else {
                        // load from stack
                        refer_vreg(call->args[i], (Reg)i);
                    }
                }

                // insert at next instruction
                it_inst++;
                // set return value (r0 -> ret)
                if (call->ret != Reg::INVALID) {
                    auto &interval = alloca_map[call->ret];
                    if (interval->state == LiveInterval::kPhyReg) {
                        builder::Move(bb->insts, it_inst, interval->phy_reg,
                                      Reg::R0);
                    } else {
                        write_vreg(call->ret, Reg::R0);
                    }
                }

                if (need_backup) {
                    for (auto r : backup_regs) {
                        Address addr = func.frame.BackupAddress(r);
                        builder::Load(bb->insts, it_inst, r, addr);
                    }
                }
                it_inst--;
            } else {
                std::vector<Reg *> RRegs = inst->RRegs();
                std::vector<Reg *> WRegs = inst->WRegs();
                if (inst->op == Instr::kSTR) {
                    for (auto &r : WRegs)
                        RRegs.push_back(r);
                    WRegs.clear();
                }

                for (auto &r : RRegs) {
                    if (*r < Reg::VREG)
                        continue;
                    *r = refer_vreg(*r);
                }
                for (auto &r : WRegs) {
                    if (*r < Reg::VREG)
                        continue;
                    it_inst++;
                    *r = write_vreg(*r);
                    it_inst--;
                }
            }
            loc.n++;
        }
        loc.bb++;
    }

    // add sp, sp, x
    x = func.frame.AllocaSize() + (9 - func.frame.saved_reg_count) * 4;
    if (legal(x))
        builder::BinaryAlu(BACK(func.end.insts), Instr::kADD, Reg::SP, Reg::SP,
                           x);
    else {
        Address *imm = new Address(std::to_string(x));
        builder::Load(BACK(func.end.insts), Reg::R7, *imm, true);
        builder::BinaryAlu(BACK(func.end.insts), Instr::kADD, Reg::SP, Reg::SP,
                           Reg::R7);
    }
    // pop rx-r7, pc
    builder::Pop(BACK(func.end.insts), (Reg)rx, Reg::R7, Reg::PC);
}

} // namespace regalloca

void RegAlloca(Asm &_asm) {
    for (auto &f : _asm.funcs) {
        regalloca::RegAllocaHelper helper(*f);
        helper.Alloca();
    }
}

} // namespace backend