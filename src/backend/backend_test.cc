
#include "error.h"
#include "backend.h"
#include <gtest/gtest.h>

using namespace backend;

TEST(Backend, Addr) {
    ASSERT_EQ(Address(Reg::SP).str(), "[SP]");
    ASSERT_EQ(Address("label").str(), "label");
    Address addr;
    addr.mode = Address::kMBaseImm;
    addr.base = Reg::R7;
    addr.offset.imm = 12;
    ASSERT_EQ(addr.str(), "[R7, #12]");
    addr.mode = Address::kMBaseReg;
    addr.offset.reg = Reg::R0;
    ASSERT_EQ(addr.str(), "[R7, R0]");
    addr.mode = Address::kMBaseRegShift;
    addr.offset.reg = Reg::R0;
    addr.offset.shift.type = Shift::kLSL;
    addr.offset.shift.imm = 2;
    ASSERT_EQ(addr.str(), "[R7, R0, LSL #2]");
    addr.mode |= Address::PreIndex;
    ASSERT_EQ(addr.str(), "[R7, R0, LSL #2]!");
    addr.mode ^= Address::PreIndex;
    addr.mode |= Address::PostIndex;
    ASSERT_EQ(addr.str(), "[R7], R0, LSL #2");
}

TEST(Backend, Builder) {
    builder::instr_list insts;
#define test_inst(s)                                                           \
    ASSERT_EQ(insts.back()->str(), s);                                         \
    insts.pop_back();

#define it insts, insts.end()

    auto vreg = [](int x) { return (Reg)(x + (int)Reg::VREG); };

    builder::Load(it, Reg::R0, Address(Reg::SP));
    test_inst("LDR R0, [SP]");
    builder::LAddr(it, Reg::R0, "label");
    test_inst("LDR R0, =label");
    builder::Store(it, Reg::R0, Address(Reg::R7));
    test_inst("STR R0, [R7]");
    builder::Push(it, Reg::R0);
    test_inst("PUSH R0");
    builder::Push(it, Reg::R0, Reg::R7, Reg::LR);
    test_inst("PUSH {R0-R7, LR}");
    builder::Pop(it, Reg::R0);
    test_inst("POP R0");
    builder::Pop(it, Reg::R0, Reg::R7, Reg::PC);
    test_inst("POP {R0-R7, PC}");
    builder::Branch(it, "bb");
    test_inst("B bb");
    builder::Branch(it, Cond(Cond::LE), "bb");
    test_inst("BLE bb");
    builder::Call(it, "func", {vreg(1), vreg(0)}, vreg(0));
    test_inst("BL func // call V1 V0 -> V0");
    builder::BranchLink(it, "func");
    test_inst("BL func");
    builder::BinaryAlu(it, Instr::kADD, Reg::R0, Reg::R1, 14);
    test_inst("ADD R0, R1, #14");
    builder::BinaryAlu(it, Instr::kSUB, Reg::SP, Reg::SP, Reg::R0);
    test_inst("SUB SP, SP, R0");
    builder::Move(it, Reg::R0, Reg::R1);
    test_inst("MOV R0, R1");
    builder::Move(it, Reg::R0, 12);
    test_inst("MOV R0, #12");
    builder::Cmp(it, Reg::R0, Reg::R1);
    test_inst("CMP R0, R1");
    builder::Cmp(it, Reg::R0, 0);
    test_inst("CMP R0, #0");
    builder::Label(it, "label");
    test_inst("label:");
}

TEST(Backend, AdvanceBuilder) {
    Func f("func");
    f.bbs.push_back(std::make_unique<BB>());
    auto &bb = f.bbs.back();
    auto &insts = bb->insts;
#define test_inst(s)                                                           \
    ASSERT_EQ(insts.back()->str(), s);                                         \
    insts.pop_back();

#ifdef it
#undef it
#endif
#define it f, insts, insts.end()

    builder::adv::Operand r1(Reg::R1), r2(Reg::R2), i1(1), i2(2);

    builder::adv::BinaryAlu(it, Instr::kMUL, r1, r1, r2);
    test_inst("MUL R1, R1, R2");
    builder::adv::BinaryAlu(it, Instr::kMUL, r1, r1, i1);
    test_inst("MUL R1, R1, #1");
    builder::adv::BinaryAlu(it, Instr::kMUL, r1, i1, r1);
    test_inst("MUL R1, R1, #1");
    builder::adv::BinaryAlu(it, Instr::kMUL, r1, i1, i2);
    test_inst("MUL R1, V0, #2");
    Shift shf;
    shf.type = Shift::kLSL;
    shf.imm = 2;
    builder::adv::BinaryAlu(it, Instr::kMUL, r1, r1, r2, true, shf);
    test_inst("MUL R1, R1, R2, LSL #2");
    builder::adv::BinaryAlu(it, Instr::kMUL, r1, r1, i1, true, shf);
    test_inst("MUL R1, R1, V1, LSL #2");

    builder::adv::Cmp(it, r1, r2);
    test_inst("CMP R1, R2");
    builder::adv::Cmp(it, r1, i1);
    test_inst("CMP R1, #1");
    builder::adv::Cmp(it, i1, i2);
    test_inst("CMP V2, #2");
    builder::adv::Cmp(it, r1, builder::adv::Operand(16777215));
    test_inst("CMP R1, V3");

    builder::adv::Move(it, r1, r2);
    test_inst("MOV R1, R2");
    builder::adv::Move(it, r1, i1);
    test_inst("MOV R1, #1");
    builder::adv::Move(it, r1, builder::adv::Operand(16777215));
    test_inst("LDR R1, [R1]");
}

TEST(Backend, TraversalRegister) {
    builder::instr_list insts;

    auto check = [&](std::set<Reg> r, std::set<Reg> w) {
        auto r1 = insts.back()->RRegs();
        auto w1 = insts.back()->WRegs();
        if (r1.size() != r.size() || w1.size() != w.size()) {
            fprintf(stderr, "size not match\n");
            return false;
        }
        for (int i = 0; i < r.size(); i++) {
            if (r.count(*r1[i]) != 1) {
                fprintf(stderr, "r not match\n");
                return false;
            }
        }
        for (int i = 0; i < w.size(); i++) {
            if (w.count(*w1[i]) != 1) {
                fprintf(stderr, "w not match\n");
                return false;
            }
        }
        return true;
    };

#define it insts, insts.end()

    auto vreg = [](int x) { return (Reg)(x + (int)Reg::VREG); };

    builder::Load(it, Reg::R0, Address(Reg::SP));
    ASSERT_TRUE(check({Reg::SP}, {Reg::R0}));
    builder::LAddr(it, Reg::R0, "label");
    ASSERT_TRUE(check({}, {Reg::R0}));
    builder::Store(it, Reg::R0, Address(Reg::R7));
    ASSERT_TRUE(check({Reg::R0, Reg::R7}, {}));
    builder::Push(it, Reg::R0);
    ASSERT_TRUE(check({Reg::R0}, {}));
    builder::Push(it, Reg::R0, Reg::R2, Reg::LR);
    ASSERT_TRUE(check({Reg::LR}, {}));
    builder::Pop(it, Reg::R0);
    ASSERT_TRUE(check({}, {Reg::R0}));
    builder::Pop(it, Reg::R0, Reg::R2, Reg::PC);
    ASSERT_TRUE(check({}, {Reg::PC}));
    builder::BinaryAlu(it, Instr::kADD, Reg::R0, Reg::R1, 14);
    ASSERT_TRUE(check({Reg::R1}, {Reg::R0}));
    builder::BinaryAlu(it, Instr::kSUB, Reg::SP, Reg::SP, Reg::R0);
    ASSERT_TRUE(check({Reg::SP, Reg::R0}, {Reg::SP}));
    builder::Move(it, Reg::R0, Reg::R1);
    ASSERT_TRUE(check({Reg::R1}, {Reg::R0}));
    builder::Move(it, Reg::R0, 12);
    ASSERT_TRUE(check({}, {Reg::R0}));
    builder::Cmp(it, Reg::R0, Reg::R1);
    ASSERT_TRUE(check({Reg::R0, Reg::R1}, {}));
    builder::Cmp(it, Reg::R0, 0);
    ASSERT_TRUE(check({Reg::R0}, {}));

    Address addr;
    addr.mode = Address::kMBaseReg;
    addr.base = Reg::R7;
    addr.offset.reg = Reg::R0;
    std::set<Reg> reg_set = {Reg::R7, Reg::R0};
    ASSERT_EQ(reg_set.size(), addr.RRegs().size());
    for (auto r : addr.RRegs()) {
        ASSERT_EQ(reg_set.count(*r), 1);
    }
}
