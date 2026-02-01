add_shad_executable(patchelf_executable main.cpp)
target_link_libraries(patchelf_executable PRIVATE elf)

add_shad_tests(test_patchelf test.cpp)
target_link_libraries(test_patchelf PRIVATE elf)
