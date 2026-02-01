add_shad_executable(strip_executable main.cpp)
target_link_libraries(strip_executable PRIVATE elf)

add_shad_executable(tester_strip tester.cpp)
target_link_libraries(tester_strip PRIVATE elf)

add_shad_tests(test_strip test.cpp)
target_link_libraries(test_strip PRIVATE elf)
target_compile_definitions(test_strip PRIVATE STRIP_PATH=\"$<TARGET_FILE:strip_executable>\")
target_compile_definitions(test_strip PRIVATE TESTER_PATH=\"$<TARGET_FILE:tester_strip>\")
add_dependencies(test_strip tester_strip)
