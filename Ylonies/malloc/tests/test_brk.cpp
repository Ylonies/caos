#include <gtest/gtest.h>

TEST(MallocTests, BRK) {
    void* current_brk = sbrk(0);
    void* ptrs[5000];
    size_t brk_calls = 0;
    for (int i = 1; i < 5000; ++i) {
        ptrs[i] = malloc(i);
        if (current_brk != sbrk(0)) {
            current_brk = sbrk(0);
            ++brk_calls;
        }
    }

    ASSERT_LE(brk_calls, 100);

    for (int i = 1; i < 5000; ++i) {
        free(ptrs[i]);
    }
}