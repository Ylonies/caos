#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

#include <gtest/gtest.h>

#ifndef STRIP_PATH
#define STRIP_PATH "./strip"
#endif

#ifndef TESTER_PATH
#define TESTER_PATH "tester"
#endif

namespace fs = std::filesystem;

TEST(StripTests, Simple) {
    std::string cp_cmd = "cp " + std::string(TESTER_PATH) + " tester_elf";
    system(cp_cmd.c_str());
    system("readelf -h tester_elf | grep 'Number of section headers' > sections_before");
    std::string strip_cmd = std::string(STRIP_PATH) + " tester_elf";

    ASSERT_EQ(system(strip_cmd.c_str()), 0) << "Strip failed";

    system("readelf -a tester_elf > readelf.out 2> warnings.out");
    auto warnings_size = fs::file_size("warnings.out");

    EXPECT_EQ(warnings_size, 0) << "readelf produced warning/errors, see warnings.out";

    system(
        "readelf -h tester_elf 2> /dev/null | grep 'Number of section headers' > sections_after");
    const int input = 7;
    const int expected = input + 5;
    std::string run_cmd = "./tester_elf " + std::to_string(input) + " > result.txt";

    EXPECT_EQ(system(run_cmd.c_str()), 0) << "Program execution failed";
    EXPECT_NE(system("grep -q '\\.debug' readelf.out"), 0) << "Debug sections were not removed";
    EXPECT_NE(system("grep -q '\\.symtab' readelf.out"), 0) << "Symtab section was not removed";
    EXPECT_NE(system("diff -q sections_before sections_after > /dev/null"), 0)
        << "No sections were actually deleted";

    std::ifstream result("result.txt");

    ASSERT_TRUE(result.is_open());

    int output;
    result >> output;

    EXPECT_EQ(output, expected) << "Program output is incorrect";

    fs::remove("tester_elf");
    fs::remove("readelf.out");
    fs::remove("result.txt");
    fs::remove("sections_before");
    fs::remove("sections_after");
}

TEST(StripTests, StripDebug) {
    std::string cp_cmd = "cp " + std::string(TESTER_PATH) + " tester_elf";
    system(cp_cmd.c_str());
    system("readelf -h tester_elf | grep 'Number of section headers' > sections_before");
    std::string strip_cmd = std::string(STRIP_PATH) + " -d tester_elf";

    ASSERT_EQ(system(strip_cmd.c_str()), 0) << "Strip failed";

    system("readelf -a tester_elf > readelf.out 2> warnings.out");

    auto warnings_size = fs::file_size("warnings.out");

    EXPECT_EQ(warnings_size, 0) << "readelf produced warning/errors, see warnings.out";

    system(
        "readelf -h tester_elf 2> /dev/null | grep 'Number of section headers' > sections_after");

    const int input = 7;
    const int expected = input + 5;
    std::string run_cmd = "./tester_elf " + std::to_string(input) + " > result.txt";

    EXPECT_EQ(system(run_cmd.c_str()), 0) << "Program execution failed";
    EXPECT_NE(system("grep -q '\\.debug' readelf.out"), 0) << "Debug sections were not removed";
    EXPECT_EQ(system("grep -q '\\.symtab' readelf.out"), 0) << "Symtab section were removed";
    EXPECT_NE(system("diff -q sections_before sections_after > /dev/null"), 0)
        << "No sections were actually deleted";

    std::ifstream result("result.txt");

    ASSERT_TRUE(result.is_open());

    int output;
    result >> output;

    EXPECT_EQ(output, expected) << "Program output is incorrect";

    fs::remove("tester_elf");
    fs::remove("readelf.out");
    fs::remove("result.txt");
    fs::remove("sections_before");
    fs::remove("sections_after");
}

TEST(StripTests, ShuffleSections) {
    std::string cp_cmd = "cp " + std::string(TESTER_PATH) + " tester_elf";
    system(cp_cmd.c_str());
    system("/usr/bin/patchelf --set-rpath /wow tester_elf");
    system("readelf -h tester_elf | grep 'Number of section headers' > sections_before");
    std::string strip_cmd = std::string(STRIP_PATH) + " -d tester_elf";

    ASSERT_EQ(system(strip_cmd.c_str()), 0) << "Strip failed";

    system("readelf -a tester_elf > readelf.out 2> warnings.out");
    auto warnings_size = fs::file_size("warnings.out");

    EXPECT_EQ(warnings_size, 0) << "readelf produced warning/errors, see warnings.out";

    system(
        "readelf -h tester_elf 2> /dev/null | grep 'Number of section headers' > sections_after");

    const int input = 7;
    const int expected = input + 5;
    std::string run_cmd = "./tester_elf " + std::to_string(input) + " > result.txt";

    EXPECT_EQ(system(run_cmd.c_str()), 0) << "Program execution failed";
    EXPECT_NE(system("grep -q '\\.debug' readelf.out"), 0) << "Debug sections were not removed";
    EXPECT_EQ(system("grep -q '\\.symtab' readelf.out"), 0) << "Symtab section were removed";
    EXPECT_NE(system("diff -q sections_before sections_after > /dev/null"), 0)
        << "No sections were actually deleted";

    std::ifstream result("result.txt");

    ASSERT_TRUE(result.is_open());

    int output;
    result >> output;

    EXPECT_EQ(output, expected) << "Program output is incorrect";

    fs::remove("tester_elf");
    fs::remove("readelf.out");
    fs::remove("result.txt");
    fs::remove("sections_before");
    fs::remove("sections_after");
}
