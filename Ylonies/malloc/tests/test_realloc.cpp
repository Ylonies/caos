#include <gtest/gtest.h>

size_t GetChunkSize(void* chunk) {
    char* ptr = static_cast<char*>(chunk);
    size_t chunk_size = *reinterpret_cast<size_t*>(ptr - sizeof(size_t));
    size_t unused_bits = 0x7;
    return chunk_size & ~unused_bits;
}

TEST(MallocTests, Realloc) {
    void* ptr = malloc(1024);  // chunk_size should be 1040
    void* ptr_copy = ptr;
    int size = GetChunkSize(ptr);
    ptr = realloc(ptr, 1030);
    ASSERT_EQ(GetChunkSize(ptr), size);
    ptr = realloc(ptr, 50);
    ASSERT_EQ(ptr, ptr_copy);
    ptr = realloc(ptr, 2000);
    ASSERT_TRUE(GetChunkSize(ptr) > 2000);
    free(ptr);

    size_t* array_ptr = (size_t*)malloc(7 * sizeof(size_t));
    ptr = malloc(100);
    for (size_t i = 0; i < 7; ++i) {
        *(array_ptr + i) = static_cast<size_t>(1 << i);
    }
    array_ptr = (size_t*)realloc(array_ptr, 11 * sizeof(size_t));
    for (size_t i = 0; i < 7; ++i) {
        ASSERT_EQ(*(array_ptr + i), static_cast<size_t>(1 << i));
    }
    free(ptr);
    free(array_ptr);
}
