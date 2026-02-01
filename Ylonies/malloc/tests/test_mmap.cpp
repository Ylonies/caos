#include <gtest/gtest.h>

TEST(MallocTests, MMAP) {
    char* ptr = static_cast<char*>(malloc(1040));
    size_t chunk_size = *reinterpret_cast<size_t*>(ptr - sizeof(size_t));
    ASSERT_EQ(chunk_size & 0x2, 0);
    ASSERT_TRUE((uintptr_t)ptr % 16 == 0);

    char* mmaped_ptr = static_cast<char*>(malloc(2 * 1024 * 1024));
    chunk_size = *reinterpret_cast<size_t*>(mmaped_ptr - sizeof(size_t));
    ASSERT_EQ(chunk_size & 0x2, 0x2);
    ASSERT_TRUE((uintptr_t)mmaped_ptr % 16 == 0);
    free(ptr);
    free(mmaped_ptr);
}