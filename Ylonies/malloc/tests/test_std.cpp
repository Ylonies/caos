#include <vector>

#include <gtest/gtest.h>

TEST(MallocTests, Vector) {
    std::vector<int> vec{1, 2, 3, 4, 5};
    for (size_t i = 0; i < vec.size(); ++i) {
        ASSERT_EQ(vec[i], i + 1);
    }
    vec.resize(1024);
    ASSERT_EQ(vec.capacity(), 1024);
}

TEST(MallocTests, New) {
    int* array = new int[10];
    int* value = new int(5);

    array[5] = 10;
    ASSERT_EQ(array[5], 10);
    ASSERT_EQ(*value, 5);

    delete value;
    delete[] array;
}