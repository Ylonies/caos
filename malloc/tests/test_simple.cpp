#include <gtest/gtest.h>

TEST(MallocTests, MallocSimple) {
    int* ptr = static_cast<int*>(malloc(sizeof(int)));
    *ptr = 5;

    ASSERT_EQ(*ptr, 5);
    free(ptr);
}