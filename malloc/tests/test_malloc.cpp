#include <gtest/gtest.h>

TEST(MallocTests, Malloc) {
    size_t size = 1040;
    void* ptr_fresh[4];
    for (int i = 0; i < 4; ++i) {
        ptr_fresh[i] = malloc(size);
        ASSERT_EQ((uintptr_t)ptr_fresh[i] % 2 * sizeof(size_t), 0);
    }
    free(ptr_fresh[1]);
    void* ptr = malloc(size);
    ASSERT_EQ(ptr, ptr_fresh[1]);
    free(ptr);
    free(ptr_fresh[0]);
    free(ptr_fresh[2]);

    ptr = malloc(2 * size);
    ASSERT_EQ(ptr, ptr_fresh[0]);
    free(ptr_fresh[3]);
    free(ptr);

    // PREV_INUSE check
    size_t* array_ptr = (size_t*)malloc(7 * sizeof(size_t));
    void* other = malloc(100);
    for (size_t i = 0; i < 7; ++i) {
        *(array_ptr + i) = static_cast<size_t>(0xfffffffffffffff0);
    }
    free(other);
    void* new_other = malloc(100);
    ASSERT_EQ(other, new_other);
    free(new_other);

    for (size_t size = 0; size < 100; ++size) {
        other = malloc(100);
        for (size_t i = 0; i < 7; ++i) {
            *(array_ptr + i) = static_cast<size_t>(size);
        }
        free(other);
        void* new_other = malloc(100);
        ASSERT_EQ(other, new_other);
        free(new_other);
    }
}