add_executable(zweidrei_tests test_simd_board.cpp test_movegen.cpp)
target_link_libraries(zweidrei_tests PRIVATE zweidrei_core gtest_main)

include(GoogleTest)
gtest_discover_tests(zweidrei_tests)
