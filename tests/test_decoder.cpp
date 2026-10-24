#include "huffman/benchmark.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using huffman::DecodeTables;
using huffman::EncodedData;
using huffman::SymbolLength;

namespace {

EncodedData pack_bits(const std::string& bits, std::uint64_t symbols) {
    EncodedData data;
    data.bytes.resize((bits.size() + 7) / 8);
    data.bit_count = bits.size();
    data.symbol_count = symbols;
    for (std::size_t i = 0; i < bits.size(); ++i) {
        if (bits[i] == '1') {
            data.bytes[i / 8] |= static_cast<std::uint8_t>(0x80u >> (i % 8));
        }
    }
    return data;
}

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Function>
void require_invalid(Function&& function, const char* message) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(message);
}

void check_all(const EncodedData& data, const DecodeTables& tables,
               const std::vector<std::uint32_t>& expected) {
    std::vector<std::uint32_t> actual;
    huffman::decode_baseline(data, tables, actual);
    require(actual == expected, "baseline output mismatch");
    huffman::decode_base(data, tables, actual);
    require(actual == expected, "merged base output mismatch");
    huffman::decode_buffer(data, tables, actual);
    require(actual == expected, "batch buffer output mismatch");
}

void check_invalid_all(const EncodedData& data, const DecodeTables& tables) {
    std::vector<std::uint32_t> actual;
    require_invalid([&] { huffman::decode_baseline(data, tables, actual); },
                    "baseline accepted invalid input");
    require_invalid([&] { huffman::decode_base(data, tables, actual); },
                    "merged base accepted invalid input");
    require_invalid([&] { huffman::decode_buffer(data, tables, actual); },
                    "batch buffer accepted invalid input");
}

}  // namespace

int main() {
    try {
        const auto abc = huffman::make_decode_tables({
            {static_cast<std::uint32_t>('C'), 2},
            {static_cast<std::uint32_t>('A'), 1},
            {static_cast<std::uint32_t>('B'), 2},
        });
        // Canonical codes: A=0, B=10, C=11. ABCA=010110.
        check_all({{0x58}, 6, 4}, abc, {'A', 'B', 'C', 'A'});
        // ABCABCA=01011010110, crossing the first byte boundary.
        check_all({{0x5A, 0xC0}, 11, 7}, abc,
                  {'A', 'B', 'C', 'A', 'B', 'C', 'A'});
        check_all({{}, 0, 0}, abc, {});

        const auto single = huffman::make_decode_tables({{'X', 1}});
        check_all({{0x00}, 3, 3}, single, {'X', 'X', 'X'});

        // A=0, B=100, C=101, D=110, E=111: many byte and batch crossings.
        const auto five = huffman::make_decode_tables(
            {{'A', 1}, {'B', 3}, {'C', 3}, {'D', 3}, {'E', 3}});
        const char* codes[] = {"0", "100", "101", "110", "111"};
        std::string bits;
        std::vector<std::uint32_t> expected;
        std::uint32_t state = 7;
        for (unsigned i = 0; i < 4096; ++i) {
            state = state * 1664525u + 1013904223u;
            const unsigned selected = state % 5;
            expected.push_back(static_cast<std::uint32_t>('A' + selected));
            bits += codes[selected];
        }
        const auto long_input = pack_bits(bits, expected.size());
        check_all(long_input, five, expected);

        // A=0, B=1 followed by 31 zeros, C=1 followed by 30 zeros and 1.
        const auto long_codes = huffman::make_decode_tables(
            {{'A', 1}, {'B', 32}, {'C', 32}});
        require(long_codes.max_length == 32, "max length was not retained");
        const std::string b_code = "1" + std::string(31, '0');
        const std::string c_code = "1" + std::string(30, '0') + "1";
        check_all(pack_bits(b_code + c_code + "0" + b_code, 4), long_codes,
                  {'B', 'C', 'A', 'B'});

        // Exercise a 32-bit code between short codes without any hybrid table.
        std::vector<std::uint32_t> mixed_lengths(18, 'A');
        mixed_lengths[1] = 'B';
        check_all(pack_bits("0" + b_code + std::string(16, '0'), 18), long_codes,
                  mixed_lengths);

        // These input-validation cases apply to all three formal decoders.
        check_all({{}, 0, 0}, huffman::make_decode_tables({}), {});
        check_invalid_all({{0x80, 0, 0, 0}, 31, 1}, long_codes);  // Truncated 32-bit code.
        check_invalid_all({{0x80, 0}, 16, 1}, single);            // Invalid prefix.
        check_invalid_all({{0}, 8, 9}, single);                  // Too many expected symbols.
        check_invalid_all({{0}, 8, 7}, single);                  // Too few expected symbols.
        check_invalid_all({{0}, 1, 1}, huffman::make_decode_tables({}));
        check_invalid_all({{0}, 0, 1}, single);                  // Empty bits, nonempty output.

        const auto timing = huffman::benchmark_decoder(
            huffman::decode_buffer, long_input, five, expected, 1, 3);
        require(timing.median_ms >= 0, "benchmark returned negative time");

        check_invalid_all({{0x80}, 1, 1}, abc);  // Starts with 1, but needs 2 bits.
        check_invalid_all({{0x58}, 6, 3}, abc);  // Wrong symbol count.
        check_invalid_all({{}, 1, 1}, abc);      // Missing byte.
        require_invalid([] { huffman::make_decode_tables({{'A', 1}, {'A', 2}}); },
                        "duplicate symbol was accepted");
        require_invalid([] { huffman::make_decode_tables({{'A', 1}, {'B', 1}, {'C', 1}}); },
                        "invalid code lengths were accepted");
        std::cout << "All decoder checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
