#pragma once

#include "huffman/types.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace huffman {

constexpr unsigned kStartBits = 12;
constexpr unsigned kMaxCodeLength = 32;

// Immutable during decoding. Create once, outside a pure decoding benchmark.
struct DecodeTables {
    std::array<std::uint64_t, kMaxCodeLength + 1> base_cwd{};
    std::array<std::uint64_t, kMaxCodeLength + 1> base_sym{};
    std::array<std::int64_t, kMaxCodeLength + 1> base{};
    std::array<std::uint64_t, kMaxCodeLength + 1> limit{};
    std::array<std::uint32_t, kMaxCodeLength + 1> count{};
    std::array<std::uint8_t, 1u << kStartBits> start{};
    std::vector<std::uint32_t> symbols;
    unsigned max_length = 0;
};

// Input order may be arbitrary. Symbols are sorted by (length, symbol_id).
// Invalid lengths and duplicate symbols throw std::invalid_argument.
DecodeTables make_decode_tables(const std::vector<SymbolLength>& lengths);

// These functions clear output, then append symbol IDs. Invalid streams throw.
void decode_baseline(const EncodedData& input, const DecodeTables& tables,
                     std::vector<std::uint32_t>& output);
void decode_base(const EncodedData& input, const DecodeTables& tables,
                 std::vector<std::uint32_t>& output);
void decode_buffer(const EncodedData& input, const DecodeTables& tables,
                   std::vector<std::uint32_t>& output);

}  // namespace huffman
