#include "backend.h"
#include <stdarg.h>

namespace backend {
namespace builder {

Word legal(Word x) {
    Word y;
    for (int i = 0; i < 32; i = i + 2) {
        y = (x >> (32 - i)) | (x << i);
        if (y < 256)
            return 1;
    }
    return 0;
}

Reg TestImm(instr_list &insts, instr_iter it, Word x) {
    if (legal(x))
        return Reg::INVALID;
    Address *addr = new Address(std::to_string(x));
    Load(insts, it, Reg::R8, *addr, true);
    return Reg::R8;
}

void Load(instr_list &insts, instr_iter it, Reg rd, Address addr, bool eq_addr,
          bool Literal_pools) {
    auto ldr = std::make_unique<LDR>();
    ldr->rd = rd;
    Reg k = Reg::INVALID;
    if ((addr.mode & addr.mode_m_mask) == 8) // Address::kMBaseImm
        k = TestImm(insts, it, addr.offset.imm);
    if (k != Reg::INVALID) {
        addr.mode = Address::kMBaseReg;
        addr.offset.reg = k;
    }
    ldr->addr = addr;
    ldr->eq_addr = eq_addr;
    ldr->Literal_pools = Literal_pools;
    insts.insert(it, std::move(ldr));
}
void LAddr(instr_list &insts, instr_iter it, Reg rd, std::string label) {
    auto ldr = std::make_unique<LDR>();
    ldr->rd = rd;
    ldr->addr = Address(label);
    ldr->eq_addr = true;
    insts.insert(it, std::move(ldr));
}

void Store(instr_list &insts, instr_iter it, Reg rd, Address addr) {
    auto str = std::make_unique<STR>();
    str->rd = rd;
    Reg k = Reg::INVALID;
    if ((addr.mode & addr.mode_m_mask) == 8) // Address::kMBaseImm
        k = TestImm(insts, it, addr.offset.imm);
    if (k != Reg::INVALID) {
        addr.mode = Address::kMBaseReg;
        addr.offset.reg = k;
    }
    str->addr = addr;
    insts.insert(it, std::move(str));
}

void Push(instr_list &insts, instr_iter it, Reg r) {
    auto push = std::make_unique<PUSH>();
    push->regs.push_back(r);
    insts.insert(it, std::move(push));
}

void Push(instr_list &insts, instr_iter it, Reg from, Reg to, Reg r) {
    auto push = std::make_unique<PUSH>();
    push->range = {from, to};
    push->regs.push_back(r);
    insts.insert(it, std::move(push));
}

void Pop(instr_list &insts, instr_iter it, Reg r) {
    auto pop = std::make_unique<POP>();
    pop->regs.push_back(r);
    insts.insert(it, std::move(pop));
}

void Pop(instr_list &insts, instr_iter it, Reg from, Reg to, Reg r) {
    auto pop = std::make_unique<POP>();
    pop->range = {from, to};
    pop->regs.push_back(r);
    insts.insert(it, std::move(pop));
}

void Branch(instr_list &insts, instr_iter it, std::string label) {
    insts.insert(it, std::make_unique<backend::Branch>(Cond(), label));
}

void Branch(instr_list &insts, instr_iter it, Cond cond, std::string label) {
    insts.insert(it, std::make_unique<backend::Branch>(cond, label));
}

void Call(instr_list &insts, instr_iter it, std::string func,
          std::vector<Reg> args, Reg ret) {
    auto call = std::make_unique<backend::Call>();
    call->func = func;
    call->args = args;
    call->ret = ret;
    insts.insert(it, std::move(call));
}

void BranchLink(instr_list &insts, instr_iter it, std::string func) {
    insts.insert(it, std::make_unique<backend::BranchLink>(func));
}

void BinaryAlu(instr_list &insts, instr_iter it, int op, Reg rd, Reg a, Reg b) {
    auto alu = std::make_unique<backend::BinaryAlu>(op);
    alu->rd = rd;
    alu->a = a;
    alu->b.r = b;
    insts.insert(it, std::move(alu));
}

void BinaryAlu(instr_list &insts, instr_iter it, int op, Reg rd, Reg a,
               Word b) {
    auto alu = std::make_unique<backend::BinaryAlu>(op);
    alu->rd = rd;
    alu->a = a;
    Reg k = TestImm(insts, it, b);
    if (k == Reg::INVALID) {
        alu->b.imm = b;
        alu->flags |= alu->kFlagBIsImm;
    } else {

        alu->b.r = k;
        alu->flags |= alu->kFlagS;
    }
    insts.insert(it, std::move(alu));
}

void Move(instr_list &insts, instr_iter it, Reg rd, Reg rs) {
    insts.insert(it, std::make_unique<MOV>(rd, rs));
}

void Move(instr_list &insts, instr_iter it, Reg rd, Word imm) {
    insts.insert(it, std::make_unique<MOV>(rd, imm));
}

void Cmp(instr_list &insts, instr_iter it, Reg a, Reg b) {
    auto cmp = std::make_unique<CMP>();
    cmp->a = a;
    cmp->b.r = b;
    insts.insert(it, std::move(cmp));
}

void Cmp(instr_list &insts, instr_iter it, Reg a, Word b) {
    auto cmp = std::make_unique<CMP>();
    cmp->a = a;
    cmp->b.imm = b;
    cmp->flags |= cmp->kFlagBIsImm;
    insts.insert(it, std::move(cmp));
}

void Label(instr_list &insts, instr_iter it, std::string label) {
    insts.insert(it, std::make_unique<LabelInstr>(label));
}

namespace adv {

int NBit(Word x) {
    int count = 0;
    while (x)
        (x >>= 1), count++;
    return count;
}

// 为立即数分配寄存器
Reg Op2Reg(Func &func, instr_list &insts, instr_iter it, Operand op, Reg rd) {
    if (op.flags & op.kIsImm) {
        if (rd == Reg::INVALID)
            rd = func.AllocaVReg();
        int nbit = NBit(op.imm); // 检查是否能通过mov直接得到立即数
        if (nbit <= 16) {
            builder::Move(insts, it, rd, op.imm);
            return rd;
        } else { // 使用load获取立即数
            auto label = func.CreateIntImm(op.imm);
            builder::Load(insts, it, rd, Address(label), true);
            builder::Load(insts, it, rd, Address(rd));
            // Load(insts, it, rd, rd);
            return rd;
        }
    } else {
        return op.r;
    }
}

void SetB(Func &func, instr_list &insts, instr_iter it, Operand op, int &flags,
          RegImmU &b) {
    if (op.flags & op.kIsImm) {
        int nbit = NBit(op.imm);
        if (nbit <= 12) {
            b.imm = op.imm;
            flags |= Instr::kFlagBIsImm;
        } else {
            b.r = Op2Reg(func, insts, it, op);
        }
    } else {
        b.r = op.r;
    }
}

void BinaryAlu(Func &func, instr_list &insts, instr_iter it, int op, Operand r,
               Operand a, Operand b, bool has_shift, Shift shift) {
    assert((r.flags & r.kIsImm) == 0);
    auto alu = std::make_unique<backend::BinaryAlu>(op);
    alu->rd = r.r;
    if (a.flags & a.kIsImm) {
        if (alu->IsCommutative() && !(b.flags & b.kIsImm)) {
            alu->a = b.r;
            b = a;
        } else {
            alu->a = Op2Reg(func, insts, it, a);
        }
    } else {
        alu->a = a.r;
    }
    if (has_shift) {
        b = Op2Reg(func, insts, it, b);
        alu->flags |= alu->kFlagHasShift;
        alu->shift = shift;
    }
    if (op == Instr::kMUL || op == Instr::kSDIV) {
        alu->b.r = Op2Reg(func, insts, it, b);
    } else
        SetB(func, insts, it, b, alu->flags, alu->b);
    insts.insert(it, std::move(alu));
}

void Cmp(Func &func, instr_list &insts, instr_iter it, Operand a, Operand b) {
    auto cmp = std::make_unique<CMP>();
    cmp->a = Op2Reg(func, insts, it, a);
    SetB(func, insts, it, b, cmp->flags, cmp->b);
    insts.insert(it, std::move(cmp));
}

void Move(Func &func, instr_list &insts, instr_iter it, Operand rd,
          Operand rs) {
    auto mov = std::make_unique<MOV>();
    Reg r = Op2Reg(func, insts, it, rd);
    Reg s = Op2Reg(func, insts, it, rs, r);
    if (s != r)
        builder::Move(insts, it, r, s);
}

} // namespace adv

} // namespace builder

} // namespace backend
