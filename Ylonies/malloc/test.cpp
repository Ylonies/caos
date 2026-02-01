#include <cstdlib>
#include <string>

#include <gtest/gtest.h>

#ifndef LIB_PATH
#define LIB_PATH "./libmalloc.so"
#endif

#ifndef TEST1_PATH
#define TEST1_PATH "./test_malloc1"
#endif

#ifndef TEST2_PATH
#define TEST2_PATH "./test_malloc2"
#endif

#ifndef TEST3_PATH
#define TEST3_PATH "./test_malloc3"
#endif

#ifndef TEST4_PATH
#define TEST4_PATH "./test_malloc4"
#endif

#ifndef TEST5_PATH
#define TEST5_PATH "./test_malloc5"
#endif

#ifndef TEST6_PATH
#define TEST6_PATH "./test_malloc6"
#endif

#ifndef TEST7_PATH
#define TEST7_PATH "./test_malloc7"
#endif

#ifndef TEST8_PATH
#define TEST8_PATH "./test_malloc8"
#endif

#ifndef TEST9_PATH
#define TEST9_PATH "./test_malloc9"
#endif

#ifndef TEST10_PATH
#define TEST10_PATH "./test_malloc10"
#endif

TEST(RunMalloc, TheOneAndOnly) {
    std::string cmd1 = std::string("LD_PRELOAD=") + LIB_PATH + " " + TEST1_PATH;
    std::string cmd2 = std::string("LD_PRELOAD=") + LIB_PATH + " " + TEST2_PATH;
    std::string cmd3 = std::string("LD_PRELOAD=") + LIB_PATH + " " + TEST3_PATH;
    std::string cmd4 = std::string("LD_PRELOAD=") + LIB_PATH + " " + TEST4_PATH;
    std::string cmd5 = std::string("LD_PRELOAD=") + LIB_PATH + " " + TEST5_PATH;
    std::string cmd6 = std::string("LD_PRELOAD=") + LIB_PATH + " " + TEST6_PATH;
    std::string cmd7 = std::string("LD_PRELOAD=") + LIB_PATH + " " + TEST7_PATH;
    std::string cmd8 = std::string("LD_PRELOAD=") + LIB_PATH + " " + TEST8_PATH;
    std::string cmd9 = std::string("LD_PRELOAD=") + LIB_PATH + " " + TEST9_PATH;
    std::string cmd10 = std::string("LD_PRELOAD=") + LIB_PATH + " " + TEST10_PATH;

    EXPECT_EQ(system(cmd1.c_str()), 0);
    EXPECT_EQ(system(cmd2.c_str()), 0);
    EXPECT_EQ(system(cmd3.c_str()), 0);
    EXPECT_EQ(system(cmd4.c_str()), 0);
    EXPECT_EQ(system(cmd5.c_str()), 0);
    EXPECT_EQ(system(cmd6.c_str()), 0);
    EXPECT_EQ(system(cmd7.c_str()), 0);
    EXPECT_EQ(system(cmd8.c_str()), 0);
    EXPECT_EQ(system(cmd9.c_str()), 0);
    EXPECT_EQ(system(cmd10.c_str()), 0);
}
