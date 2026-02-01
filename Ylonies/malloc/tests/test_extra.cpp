#include <gtest/gtest.h>

TEST(MallocTests, MallocExtra) {
    // 1. We check that for small allocations, only brk() syscall is used.
    void* current_brk = sbrk(0);
    void* ptrs[1000];
    for (int i = 1; i < 1000; ++i) {
        ptrs[i] = malloc(i);
    }

    // 2. Test that we did not asked too much from OS, i.e. brk has been moved not
    // too far
    //   We have allocated totally 1000*1001/2 = 500'500 bytes,
    //   and we assume that brk has moved not too further than this number
    ASSERT_TRUE(((char*)sbrk(0) - (char*)current_brk < 550'000));

    // 3. Test that memory is indeed allocated, i.e. we can write into it
    for (int i = 1; i < 1000; ++i) {
        for (int j = 0; j < i; ++j) {
            *(static_cast<char*>(ptrs[i]) + j) = 'a';
        }
    }

    // 4. Test that malloc always returns aligned memory
    for (int i = 1; i < 1000; ++i) {
        ASSERT_TRUE(((uint64_t)ptrs[i] % alignof(std::max_align_t) == 0));
    }

    // 5. Check that memory is reused, i.e. repeated allocations of the same sizes
    //  will not call brk() again
    current_brk = sbrk(0);
    for (int i = 1; i < 1000; ++i) {
        free(ptrs[i]);
    }
    for (int i = 999; i >= 1; --i) {
        ptrs[i] = malloc(i);
    }
    ASSERT_EQ(current_brk, sbrk(0));

    // 6. Check that if I free() and then immediately malloc() the same size,
    //  I get the same address
    for (int i = 1; i < 500; ++i) {
        void* oldptr = ptrs[i];
        free(ptrs[i]);
        ptrs[i] = malloc(i);
        ASSERT_EQ(oldptr, ptrs[i]);
    }

    // 7. Check sizes of bins.
    // Bin size should be stored just before the pointer returned by malloc.
    //
    for (int i = 1; i < 500; ++i) {
        char* bin_size_ptr = static_cast<char*>(ptrs[i]) - sizeof(size_t);
        size_t bin_size = *reinterpret_cast<size_t*>(bin_size_ptr);

        // Bin sizes should be multiples of 16,
        // but last bit in the number indicates if the bin is occupied.
        ASSERT_TRUE(bin_size & 0x1);
        bin_size &= ~0x1;
        ASSERT_TRUE(bin_size % 16 == 0);

        // Bin sizes should not be too big.
        ASSERT_TRUE(bin_size > (size_t)i);
        ASSERT_TRUE(i < 32 || bin_size - i <= 32);
    }

    // 8. Check that memory from smaller bins can be reused for bigger sizes.
    current_brk = sbrk(0);

    for (int i = 1; i < 500; ++i) {
        free(ptrs[i]);
    }

    // We freed 500*501/2 = 125'250 bytes,
    // it should be enough for allocating 150*700 bytes more without calling brk()
    void* newptrs[150];
    for (int i = 0; i < 150; ++i) {
        newptrs[i] = malloc(700);
    }

    ASSERT_EQ(current_brk, sbrk(0));

    for (int i = 0; i < 150; ++i) {
        free(newptrs[i]);
    }

    // 9. And now check that this memory again can be reused for small bins
    for (int i = 1; i < 500; ++i) {
        ptrs[i] = malloc(i);
    }
    ASSERT_EQ(current_brk, sbrk(0));

    // 10. Test realloc. First, check that if we extend memory not exceeding bins
    // size,
    //   then realloc will not move it to anywhere
    for (int i = 1; i < 1000; ++i) {
        void* oldptr = ptrs[i];
        size_t increase = i % 8 != 0 ? 8 - i % 8 : 0;
        size_t newsize = i + increase;
        ptrs[i] = realloc(ptrs[i], newsize);
        ASSERT_EQ(oldptr, ptrs[i]);
    }
    for (int i = 1; i < 1000; ++i) {
        free(ptrs[i]);
    }

    // 11. Check that realloc is indeed able to reallocate
    {
        void* ptr = malloc(50);
        size_t bin_size = *reinterpret_cast<size_t*>((static_cast<char*>(ptr) - sizeof(size_t)));
        ASSERT_TRUE(bin_size % 16 == 1);  // Last bit indicates that bin is occupied
        ASSERT_TRUE(bin_size < 80);       // Likely to be 65
        for (int i = 0; i < 50; ++i) {
            *(static_cast<char*>(ptr) + i) = 'a';
        }

        void* newptr = realloc(ptr, 120);
        size_t new_bin_size =
            *reinterpret_cast<size_t*>((static_cast<char*>(newptr) - sizeof(size_t)));
        ASSERT_TRUE(new_bin_size > 80);
        ASSERT_TRUE(new_bin_size < 150);  // Likely to be 129
        ASSERT_TRUE(new_bin_size % 16 == 1);

        for (int i = 0; i < 50; ++i) {
            char c = *(static_cast<char*>(newptr) + i);
            ASSERT_TRUE(c == 'a');
        }

        // For some reason, for std::realloc this doesn't hold, so we skip this
        // check
        /*if (ptr != newptr) {
            size_t bin_size = (size_t&) *((char*) ptr - sizeof(size_t));
            assert(bin_size % 16 == 0);
        }*/

        free(newptr);
    }
}