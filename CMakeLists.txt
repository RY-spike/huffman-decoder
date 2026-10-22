cmake_minimum_required(VERSION 3.20)
project(huffman_decoder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(huffman_decoder
    src/decoder_baseline.cpp
    src/benchmark.cpp
)
target_include_directories(huffman_decoder PUBLIC include)
if(MSVC)
    target_compile_options(huffman_decoder PRIVATE /W4 /permissive-)
else()
    target_compile_options(huffman_decoder PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_executable(test_decoder tests/test_decoder.cpp)
target_link_libraries(test_decoder PRIVATE huffman_decoder)
add_test(NAME decoder_correctness COMMAND test_decoder)
