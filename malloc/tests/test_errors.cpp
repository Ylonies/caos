#include <gtest/gtest.h>

TEST(MallocTests, Errors) {
    ASSERT_DEATH(
        [] {
            void* ptr = malloc(100);
            free((char*)ptr + 4);
        }(),
        ".*");  // invalid pointer
    ASSERT_DEATH(
        [] {
            void* ptr = malloc(100);
            free(ptr);
            free(ptr);
        }(),
        ".*");  // double free
}