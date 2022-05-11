
#include "error.h"
#include "frontend.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <sstream>
#include <fstream>

using recursive_directory_iterator =
    std::filesystem::recursive_directory_iterator;

// #define GENERATE_TESTCASES

void TestDir(std::filesystem::path base) {
    std::filesystem::path testcases_in_dir = base / "in";
    std::filesystem::path testcases_out_dir = base / "out";

    std::vector<std::filesystem::path> files_in_directory;
    std::copy(std::filesystem::directory_iterator(testcases_in_dir),
              std::filesystem::directory_iterator(),
              std::back_inserter(files_in_directory));
    std::sort(files_in_directory.begin(), files_in_directory.end());

    for (const auto &path : files_in_directory) {
        auto _tmp = path.filename().string();
        if (_tmp.back() != 'y')
            continue;
        _tmp.pop_back(), _tmp.pop_back();
        auto expected_path = testcases_out_dir / (_tmp + "ll");
#ifdef GENERATE_TESTCASES
        auto ir_path = expected_path;
#else
        auto ir_path = std::filesystem::temp_directory_path() /
                       std::to_string(std::rand());
#endif
        std::ofstream ir_out(ir_path);

        std::cout << path << std::endl;
        FILE *fin = fopen(path.c_str(), "r");
        auto comp_unit = Parse(fin);
        fclose(fin);
        comp_unit = SemanticCheck(comp_unit);
        auto m = AstToIr(comp_unit);
        m->dump(ir_out);
        ir_out.close();
#ifndef GENERATE_TESTCASES
        char cmd_buf[0x1000] = {0};
        snprintf(cmd_buf, 0x1000 - 1, "diff --color=always -b -B %s %s",
                 ir_path.c_str(), expected_path.c_str());
        int err = system(cmd_buf);
        std::filesystem::remove(ir_path);
        ASSERT_EQ(0, err);
#else
        char cmd_buf[0x1000] = {0};
        snprintf(cmd_buf, 0x1000 - 1, "opt %s -S", ir_path.c_str());
        int err = system(cmd_buf);
        ASSERT_EQ(0, err);
#endif
    }
}

TEST(AstToIr, GlobalArray) {
    TestDir("../../../testcases/ast_to_ir/global_array");
}

TEST(AstToIr, All) { TestDir("../../../testcases/ast_to_ir"); }
