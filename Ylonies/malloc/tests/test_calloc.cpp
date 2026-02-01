#include <gtest/gtest.h>

TEST(MallocTests, Calloc) {
    for (int i = 0; i < 100; ++i) {
        char* str = static_cast<char*>(calloc(256, sizeof(char)));
        for (int i = 0; i < 256; ++i) {
            ASSERT_EQ(str[i], 0);
            str[i] = i;
        }
        free(str);
    }
}