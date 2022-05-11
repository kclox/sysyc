#include "ir/ir.hpp"
#include <gtest/gtest.h>

TEST(IR, Type) {
    // auto maketype = [](int type, int array_type = -1, int array_size = 0,
    //                    int ptr = -1) {
    //     ir::Type t;
    //     t.type = type;
    //     if (type == ir::Type::kTArray) {
    //         t.extra.array.type = new ir::Type(array_type);
    //         t.extra.array.size = array_size;
    //     }
    //     if (type == ir::Type::kTPtr) {
    //         t.extra.ptr.type = new ir::Type(ptr);
    //     }
    //     return t;
    // };
    // ASSERT_EQ(maketype(ir::Type::kTVoid).Str(), "void");
    // ASSERT_EQ(maketype(ir::Type::kTI32).Str(), "i32");
    // ASSERT_EQ(maketype(ir::Type::kTI1).Str(), "i1");
    // ASSERT_EQ(maketype(ir::Type::kTLabel).Str(), "label");
    // ASSERT_EQ(maketype(ir::Type::kTArray, ir::Type::kTI32, 4).Str(),
    //           "[4 x i32]");
    // ASSERT_EQ(maketype(ir::Type::kTPtr, 0, 0, ir::Type::kTI32).Str(), "i32
    // *");

    // ir::FuncType ft;
    // ft.ret_type = ir::Type();
    // ft.ret_type.type = ir::Type::kTI32;
    // ir::Type t1, t2;
    // t1.type = ir::Type::kTI32;
    // t2.type = ir::Type::kTPtr;
    // t2.extra.ptr.type = new ir::Type(ir::Type::kTI32);
    // ft.arg_types.push_back(t1);
    // ft.arg_types.push_back(t2);
    // ASSERT_EQ(ft.Str(), "i32 (i32, i32 *)");
}
