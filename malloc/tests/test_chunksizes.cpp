#include <gtest/gtest.h>

size_t GetChunkSize(void* chunk) {
    char* ptr = static_cast<char*>(chunk);
    size_t chunk_size = *reinterpret_cast<size_t*>(ptr - sizeof(size_t));
    size_t unused_bits = 0x7;
    return chunk_size & ~unused_bits;
}

// glhf
TEST(MallocTests, ChunkSizes) {
    for (size_t size = sizeof(size_t); size < 1024UL; size += sizeof(size_t)) {
        void* ptr = malloc(size);
        size_t chunk_size = GetChunkSize(ptr);
        size_t expected_size = std::max(32UL, ((size + 8) + 15) / 16 * 16);
        ASSERT_EQ(chunk_size, expected_size);
        free(ptr);
    }
}