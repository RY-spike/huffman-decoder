#pragma once

#include <cstdint>
#include <vector>

namespace huffman {

// One symbol and its Huffman code length. The decoder assigns canonical codes.
struct SymbolLength {
    std::uint32_t symbol_id;
    std::uint8_t bit_length;
};

// MSB-first bits within each byte; bits after bit_count are ignored.
struct EncodedData {
    std::vector<std::uint8_t> bytes;
    std::uint64_t bit_count = 0;
    std::uint64_t symbol_count = 0;
};

}  // namespace huffman
