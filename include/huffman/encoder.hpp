#pragma once

#include "huffman/types.hpp"

#include <cstdint>
#include <vector>

namespace huffman {

struct EncodeResult {
    std::vector<SymbolLength> lengths;
    EncodedData data;
};

// Byte-symbol canonical Huffman encoder. Equal frequencies are ordered by
// minimum subtree symbol ID. Empty input produces an empty codebook/stream.
// A single symbol uses code 0 of length 1. Throws std::invalid_argument if
// the generated tree needs a code longer than 32 bits (no length limiting).
EncodeResult encode_bytes(const std::vector<std::uint8_t>& input);

}  // namespace huffman
