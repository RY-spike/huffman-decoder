#pragma once

#include "huffman/decoder.hpp"

#include <cstdint>
#include <vector>

namespace huffman {

using Decoder = void (*)(const EncodedData&, const DecodeTables&,
                         std::vector<std::uint32_t>&);

struct BenchmarkResult {
    double median_ms = 0;
    double ns_per_symbol = 0;
    double million_symbols_per_second = 0;
};

// Excludes table construction and file I/O. Reuses one output allocation.
// Throws if the decoder output differs from the provided reference.
BenchmarkResult benchmark_decoder(Decoder decoder, const EncodedData& input,
                                  const DecodeTables& tables,
                                  const std::vector<std::uint32_t>& reference,
                                  unsigned warmup_runs = 3,
                                  unsigned measured_runs = 30);

}  // namespace huffman
