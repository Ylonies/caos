add_shad_executable(du_executable main.cpp)

add_shad_tests(test_du test.cpp)
target_compile_definitions(test_du PRIVATE DU_PATH=\"$<TARGET_FILE:du_executable>\")
