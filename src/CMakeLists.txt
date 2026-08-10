add_library(zweidrei_core
    simd_board.cpp
    attacks.cpp
    movegen.cpp
    zobrist.cpp
    tt.cpp
    evaluate.cpp
    search.cpp
    uci.cpp
)
target_include_directories(zweidrei_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)

add_executable(zweidrei main.cpp)
target_link_libraries(zweidrei PRIVATE zweidrei_core Threads::Threads)
