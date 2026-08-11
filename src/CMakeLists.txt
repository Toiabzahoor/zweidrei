add_library(zweidrei_core
    core/simd_board.cpp
    movegen/attacks.cpp
    movegen/movegen.cpp
    movegen/zobrist.cpp
    search/tt.cpp
    eval/evaluate.cpp
    eval/nnue.cpp
    search/search.cpp
    uci/uci.cpp
)
target_include_directories(zweidrei_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/movegen
    ${CMAKE_CURRENT_SOURCE_DIR}/search
    ${CMAKE_CURRENT_SOURCE_DIR}/eval
    ${CMAKE_CURRENT_SOURCE_DIR}/uci
)

find_package(Threads REQUIRED)

add_executable(zweidrei main.cpp)
target_link_libraries(zweidrei PRIVATE zweidrei_core Threads::Threads)
target_link_options(zweidrei PRIVATE -static)
