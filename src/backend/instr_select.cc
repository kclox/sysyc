#include "backend.h"
#include "dbg.hpp"

namespace backend {

extern bool NotAllowTy;

#define BACK(v) v, (v).end()

const std::string DumpVregCode = "dump-vreg-code";
const std::string DbgConvertCall = "convert-call";
const std::string DbgConvertPhi = "convert-phi";

struct InstrSelectHelper {                // 选择指令，完成转化
    ir::Module &m;                        // LLVM IR
    Asm &_asm;                            // ARM
    Func *func;                           // 当前函数对象
    Define *def;                          // 当前全局声明
    BB *bb;                               // 当前基本块
    std::map<ir::Value *, Reg> v_to_vreg; // 变量与寄存器映射表

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

void InstrSelectHelper ::VRegReset() { v_to_vreg.clear(); }

// Value  -- 变量
Reg InstrSelectHelper ::GetVReg(
    std::shared_ptr<ir::Value> v) { // 变量与寄存器映射
    if (!v)                         // 空
        return func->AllocaVReg();
    Reg vr;
    if (v->kind == ir::Value::kImm) { // 立即数
        vr = builder::adv::Op2Reg(*func, BACK(bb->insts), GetOperand(v));
        v_to_vreg.insert({v.get(), vr});
        return vr;
    }

    auto it = v_to_vreg.find(v.get()); // 根据值找寄存器
    if (it != v_to_vreg.end())
        return it->second;

    // 未找到

    vr = func->AllocaVReg();
    v_to_vreg.insert({v.get(), vr});
    return vr;
}

int InstrSelectHelper ::IrAluOpToAsmOp(int op) {
    static std::map<int, int> m{
        {ir::Instr::kOpAdd, Instr::kADD}, {ir::Instr::kOpSub, Instr::kSUB},
        {ir::Instr::kOpMul, Instr::kMUL}, {ir::Instr::kOpSdiv, Instr::kSDIV},
        {ir::Instr::kOpAnd, Instr::kAND}, {ir::Instr::kOpOr, Instr::kOR},
    };
    return m[op];
}

Reg InstrSelectHelper ::GetGlobalAdrr(std::shared_ptr<ir::Value> g) {
    auto var = std::dynamic_pointer_cast<ir::GlobalVar>(g);
    Reg rd = GetVReg();
    builder::Load(BACK(bb->insts), rd, Address(var->name), true);
    return rd;
}

Reg InstrSelectHelper ::LoadGlobal(Reg rt, std::shared_ptr<ir::Value> g) {
    Reg rd = GetGlobalAdrr(g); // LDR rd =name
    builder::Load(BACK(bb->insts), rt, Address(rd));
    return rt; /// ?
}

void InstrSelectHelper ::StoreGlobal(std::shared_ptr<ir::Value> g,
                                     Reg rd) { //******
    auto var = std::dynamic_pointer_cast<ir::GlobalVar>(g);
    Reg rtemp = GetVReg();
    builder::Load(BACK(bb->insts), rtemp, Address(var->name),
                  true);                                 // LDR rtemp =name
    builder::Store(BACK(bb->insts), rd, Address(rtemp)); // STR rd [rtemp]
}

builder::adv::Operand
InstrSelectHelper ::GetOperand(std::shared_ptr<ir::Value> v) {
    if (v->kind == ir::Value::kImm) {
        return builder::adv::Operand(
            std::dynamic_pointer_cast<ir::ImmValue>(v)->imm);
    } else if (v->kind == ir::Value::kGlobalVar) {
        return builder::adv::Operand(GetGlobalAdrr(v));
    } else if (v->kind == ir::Value::kTmpVar ||
               v->kind == ir::Value::kLocalVar) {
        return builder::adv::Operand(GetVReg(v));
    }
}

builder::adv::Operand InstrSelectHelper ::GetOperand(Reg r) {
    return builder::adv::Operand(r);
}

builder::adv::Operand InstrSelectHelper ::GetOperand(Word imm) {
    return builder::adv::Operand(imm);
}

void InstrSelectHelper ::ConvertBinaryAlu(ir::BinaryAlu *alu) {
    auto r = GetOperand(alu->result), a = GetOperand(alu->l),
         b = GetOperand(alu->r);
    if (alu->op == ir::Instr::kOpSrem) {
        // l % r == l - l / r
        builder::adv::BinaryAlu(*func, BACK(bb->insts), Instr::kSDIV, r, a, b);
        builder::adv::BinaryAlu(*func, BACK(bb->insts), Instr::kMUL, r, r, b);
        builder::adv::BinaryAlu(*func, BACK(bb->insts), Instr::kSUB, r, a, r);
    } else {
        builder::adv::BinaryAlu(*func, BACK(bb->insts), IrAluOpToAsmOp(alu->op),
                                r, a, b);
    }
}

void InstrSelectHelper ::ConvertOtherInst(ir::Instr *inst) {
    switch (inst->op) {
    case ir::Instr::kOpCall: {
        auto call = dynamic_cast<ir::Call *>(inst);
        int argc = call->args.size();
        func->frame.max_call_arg_count =
            std::max(func->frame.max_call_arg_count, argc);
        Reg ret = Reg::INVALID;
        std::vector<Reg> args;
        for (auto arg : call->args) {
            args.push_back(GetVReg(arg));
        }
        if (call->result)
            ret = GetVReg(call->result);
        builder::Call(BACK(bb->insts), call->func->name, args, ret);
        if (DbgEnabled(DbgConvertCall)) {
            fprintf(stderr, "call %s -> %s\n", call->func->name.c_str(),
                    Reg2Str(ret).c_str());
        }
    } break;
    case ir::Instr::kOpRet: {
        auto ret = dynamic_cast<ir::Ret *>(inst);
        if (ret->retval) {
            builder::adv::Move(*func, BACK(bb->insts), GetOperand(Reg::R0),
                               GetOperand(ret->retval));
        }
        builder::Branch(BACK(bb->insts), "__fend__" + func->name);
        bb->succs.push_back(&func->end);
        func->end.preds.push_back(bb);
    } break;
    case ir::Instr::kOpLoad: {
        auto load = dynamic_cast<ir::Load *>(inst);
        if (load->ptr->kind == ir::Value::kGlobalVar) {
            LoadGlobal(GetVReg(load->result), load->ptr);
        } else {
            builder::Load(BACK(bb->insts), GetVReg(load->result),
                          Address(GetVReg(load->ptr)));
        }
    } break;
    case ir::Instr::kOpStore: {
        auto store = dynamic_cast<ir::Store *>(inst);
        if (store->ptr->kind == ir::Value::kGlobalVar) {
            StoreGlobal(store->ptr, GetVReg(store->val));
        } else {
            builder::Store(BACK(bb->insts), GetVReg(store->val),
                           Address(GetVReg(store->ptr)));
        }
    } break;
    case ir::Instr::kOpAlloca: {
        auto alloca = dynamic_cast<ir::Alloca *>(inst);
        builder::BinaryAlu(
            BACK(bb->insts), Instr::kADD, GetVReg(alloca->result),
            func->frame.local_var_base,
            func->frame.AllocaVar(alloca->ty->size(), alloca->alignment));
    } break;
    case ir::Instr::kOpZext:
    case ir::Instr::kOpBitcast: {
        v_to_vreg[inst->Result().get()] = GetVReg((*inst->RValues().front()));
    } break;
    }
}

void InstrSelectHelper ::ConvertGetelementPtr(
    ir::Getelementptr *getelementptr) {
    auto rd = GetOperand(getelementptr->result);
    auto base = GetOperand(getelementptr->ptr);

    auto set_index = [&](std::shared_ptr<ir::Value> idx, int size) {
        if (idx->kind == ir::Value::kImm) {
            auto offset =
                size * std::dynamic_pointer_cast<ir::ImmValue>(idx)->imm;
            if (offset != 0) {
                builder::adv::BinaryAlu(*func, BACK(bb->insts), Instr::kADD, rd,
                                        base, GetOperand(offset));
            } else {
                builder::adv::Move(*func, BACK(bb->insts), rd, base);
            }
        } else {
            int shift = ShiftCount(size);
            if (shift) {
                builder::adv::BinaryAlu(*func, BACK(bb->insts), Instr::kADD, rd,
                                        base, GetVReg(idx), true,
                                        Shift{Shift::kLSL, shift});
            } else {
                builder::adv::Operand t = rd;
                if (rd.r == base.r)
                    t = builder::adv::Operand(func->AllocaVReg());
                builder::adv::BinaryAlu(*func, BACK(bb->insts), Instr::kMUL, t,
                                        GetVReg(idx), GetOperand(size));
                builder::adv::BinaryAlu(*func, BACK(bb->insts), Instr::kADD, rd,
                                        base, t);
            }
        }
    };

    auto idx = getelementptr->indices[0];
    set_index(idx, getelementptr->ty->size());

    if (!NotAllowTy) //
        printf("0 size : %d ty: %d\n", getelementptr->ty->size(),
               getelementptr->ty->kind);

    if (getelementptr->indices.size() >= 2) {
        base = rd;
        idx = getelementptr->indices[1];
        auto arr_ty = getelementptr->ty->cast<ir::ArrayT>();

        if (!NotAllowTy) //
            printf("1 size : %d ty: %d\n", arr_ty->element->size(),
                   arr_ty->element->kind);
        set_index(idx, arr_ty->element->size());
    }
}

int InstrSelectHelper ::ShiftCount(Word x) {
    if (x == 0)
        return 0;
    Word n = 1;
    int shift = 0;
    while (n < x && n & x)
        (n <<= 1), shift++;
    return n == x ? shift : 0;
}

Cond InstrSelectHelper ::IrCond2AsmCond(int cond) {
    static std::map<int, int> m{
        {ir::Icmp::kEq, Cond::EQ},  {ir::Icmp::kNe, Cond::NE},
        {ir::Icmp::kUgt, Cond::HI}, {ir::Icmp::kUge, Cond::HS},
        {ir::Icmp::kUlt, Cond::LO}, {ir::Icmp::kUle, Cond::LS},
        {ir::Icmp::kSgt, Cond::GT}, {ir::Icmp::kSge, Cond::GE},
        {ir::Icmp::kSlt, Cond::LT}, {ir::Icmp::kSle, Cond::LE},
    };
    return Cond(m[cond]);
}

Reg InstrSelectHelper ::ConvertIcmpI32(ir::Icmp *icmp) {
    builder::adv::Cmp(*func, BACK(bb->insts), GetOperand(icmp->l),
                      GetOperand(icmp->r));

    Reg rd = GetVReg(icmp->result);
    auto lout = ".L_cmp_out_" + std::to_string((int)rd),
         l1 = ".L_cmp_1_" + std::to_string((int)rd);
    builder::Branch(BACK(bb->insts), IrCond2AsmCond(icmp->cond), l1);
    builder::Move(BACK(bb->insts), rd, 0);
    builder::Branch(BACK(bb->insts), Cond(), lout);
    builder::Label(BACK(bb->insts), l1);
    builder::Move(BACK(bb->insts), rd, 1);
    builder::Label(BACK(bb->insts), lout);
    return rd;
}

std::string InstrSelectHelper ::GetBBLabel(std::shared_ptr<ir::LocalValue> l) {
    return ".L_" + func->name + "_bb_" + l->var();
}

void InstrSelectHelper ::Build() { // 进行转化

    for (auto &def : m.defs) {
        _asm.defs.push_back(std::make_unique<Define>());
        this->def = _asm.defs.back().get();

        this->def->is_const = def->is_const;
        this->def->name = def->var->name;
        this->def->init = std::move(def->init);
    }

    for (auto &f : m.funcs) {

        std::map<ir::BB *, std::vector<ir::Phi *>> phis; // 基本块与指令的映射

        // first: br refer count, second: alu refer count
        std::map<std::shared_ptr<ir::Value>, std::pair<int, int>> icmp_refcount;
        std::map<std::shared_ptr<ir::Value>, Cond> icmp_cond;
        std::vector<BB *> ir_bb_to_asm_bb;

        // build function
        _asm.funcs.push_back(
            std::make_unique<Func>(f->name)); // 初始化当前函数块
        this->func = _asm.funcs.back().get(); // 得到当前函数指针

        // clear vreg allocator
        v_to_vreg.clear(); // 初始化 ？

        for (auto &arg : f->args) { // 将传递的参数与寄存器建立映射
            this->func->args.push_back(GetVReg(arg));
        }

        this->func->frame.spilled_arg_count = std::max(
            (int)f->args.size() - this->func->frame.max_reg_arg_count, 0);

        this->func->has_ret = (f->ft->ret->kind != ir::Type::kVoid);
        f->ResetBBID();
        f->ResetTmpVar();

        // first traversal, create bb, collecting icmp info
        for (auto &bb : f->bblocks) {

            this->func->bbs.push_back(std::make_unique<BB>());
            this->bb = this->func->bbs.back().get();
            this->bb->ir_bb = bb.get();

            if (bb->label) // 基本块标签
                this->bb->label = GetBBLabel(bb->label);

            assert(bb->id == ir_bb_to_asm_bb.size());
            ir_bb_to_asm_bb.push_back(this->bb);

            for (auto &inst : bb->insts) {

                if (inst->op == ir::Instr::kOpIcmp) { // Icmp
                    auto res = inst->Result();
                    icmp_refcount[res] = {0, 0};
                }
                for (auto p : inst->RValues()) {
                    auto &v = *p;
                    auto it = icmp_refcount.find(v);
                    if (it != icmp_refcount.end()) {
                        if (inst->op == ir::Instr::kOpBr) {
                            it->second.first++;
                        } else {
                            it->second.second++;
                        }
                    }
                }
            }
        }

        // second traversal, convert instr
        for (auto &bb : f->bblocks) {
            this->bb = ir_bb_to_asm_bb[bb->id];

            for (auto &inst : bb->insts) {
                if (inst->IsBinaryAlu()) { // 运算指令
                    ConvertBinaryAlu(dynamic_cast<ir::BinaryAlu *>(inst.get()));
                } else if (inst->op == ir::Instr::kOpIcmp) { // Icmp
                    auto &refcount = icmp_refcount[inst->Result()];
                    auto icmp = dynamic_cast<ir::Icmp *>(inst.get());
                    if (refcount.second > 0) {
                        ConvertIcmpI32(icmp);
                    } else {
                        builder::adv::Cmp(*func, BACK(this->bb->insts),
                                          GetOperand(icmp->l),
                                          GetOperand(icmp->r));
                        icmp_cond[icmp->result] =
                            IrCond2AsmCond(icmp->cond).Not();
                    }
                } else if (inst->op == ir::Instr::kOpPhi) { // phi
                    phis[bb.get()].push_back(
                        dynamic_cast<ir::Phi *>(inst.get()));
                } else if (inst->op == ir::Instr::kOpGetelementptr) {
                    ConvertGetelementPtr(
                        dynamic_cast<ir::Getelementptr *>(inst.get()));
                } else if (inst->op == ir::Instr::kOpBr) { // br
                    auto br = dynamic_cast<ir::Br *>(inst.get());
                    if (br->cond) {
                        // 若l2在l1前，会出现问题
                        int l1_int = atoi(br->l1->var().c_str());
                        int l2_int = atoi(br->l2->var().c_str());
                        if (l2_int < l1_int) {
                            swap(br->l1, br->l2);
                            icmp_cond[br->cond] = icmp_cond[br->cond].Not();
                        }
                        this->bb->SetBranch(icmp_cond[br->cond],
                                            GetBBLabel(br->l2));
                        auto bb2 =
                            ir_bb_to_asm_bb[f->label_map[br->l2.get()]->id];

                        this->bb->succs.push_back(bb2);
                        bb2->preds.push_back(this->bb);
                    } else {
                        // check next bb is equal to label or not
                        auto need_branch = [&]() -> bool {
                            if (bb->id + 1 < func->bbs.size()) {
                                auto &next_bb = f->bblocks[bb->id + 1];
                                return next_bb->label != br->l1;
                            }
                            return true;
                        };
                        if (need_branch())
                            this->bb->SetBranch(Cond(), GetBBLabel(br->l1));
                    }
                    auto bb1 = ir_bb_to_asm_bb[f->label_map[br->l1.get()]->id];
                    this->bb->succs.push_back(bb1);
                    bb1->preds.push_back(this->bb);
                }

                else {
                    ConvertOtherInst(inst.get());
                }
            }
        }

        // convert phi instr
        for (auto &i : phis) {
            for (auto &phi : i.second) {
                auto rd = GetOperand(phi->result);
                if (DbgEnabled(DbgConvertPhi)) {
                    std::cerr << "PHI -> " + Reg2Str(rd.r) << "\n";
                }
                for (auto &pv : phi->vals) {
                    auto bb = ir_bb_to_asm_bb[f->label_map[pv.label.get()]->id];
                    if (DbgEnabled(DbgConvertPhi)) {
                        fprintf(stderr, "%s -> %s\n", pv.label->str().c_str(),
                                bb->label.c_str());
                    }
                    builder::adv::Move(*func, BACK(bb->insts), rd,
                                       GetOperand(pv.val));
                }
            }
        }
    }
}

void InstrSelect(ir::Module &m, Asm &_asm) {
    InstrSelectHelper helper(m, _asm);
    helper.Build();
    if (DbgEnabled(DumpVregCode)) {
        _asm.dump(std::cerr);
    }
}

} // namespace backend