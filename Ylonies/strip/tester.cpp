#include <iostream>
#include <vector>

int Foo(int x) {
    std::vector<int> vec;
    vec.push_back(x);
    return vec[0] + 5;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::runtime_error("Wrong argc");
    }
    int x = std::atoi(argv[1]);
    std::cout << Foo(x);
}