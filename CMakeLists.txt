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

add_library(huffman_encoder src/encoder.cpp)
target_include_directories(huffman_encoder PUBLIC include)
if(MSVC)
    target_compile_options(huffman_encoder PRIVATE /W4 /permissive-)
else()
    target_compile_options(huffman_encoder PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(test_encoder_integration tests/test_encoder_integration.cpp)
target_link_libraries(test_encoder_integration PRIVATE huffman_encoder huffman_decoder)
add_test(NAME encoder_integration COMMAND test_encoder_integration)

add_executable(benchmark_files benchmarks/benchmark_files.cpp)
target_link_libraries(benchmark_files PRIVATE huffman_encoder huffman_decoder)
if(MINGW)
    # benchmark_files uses wmain on Windows, including MinGW builds.
    target_link_options(benchmark_files PRIVATE -municode)
endif()
target_compile_definitions(benchmark_files PRIVATE HUFFMAN_BUILD_CONFIG="$<CONFIG>")
if(MSVC)
    target_compile_options(benchmark_files PRIVATE /W4 /permissive-)
else()
    target_compile_options(benchmark_files PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(test_benchmark tests/test_benchmark.cpp)
target_link_libraries(test_benchmark PRIVATE huffman_decoder)
add_test(NAME benchmark_statistics COMMAND test_benchmark)
add_test(NAME benchmark_cli COMMAND ${CMAKE_COMMAND}
    "-DBENCHMARK_EXE=$<TARGET_FILE:benchmark_files>"
    "-DINPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/data/benchmark_sample.txt"
    "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/benchmark-cli-tests"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_benchmark_cli.cmake")
