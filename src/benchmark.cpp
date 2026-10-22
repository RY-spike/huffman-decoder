#include "huffman/benchmark.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <vector>

namespace huffman {

BenchmarkResult benchmark_decoder(Decoder decoder, const EncodedData& input,
                                  const DecodeTables& tables,
                                  const std::vector<std::uint32_t>& reference,
                                  unsigned warmup_runs,
                                  unsigned measured_runs) {
    if (decoder == nullptr || measured_runs == 0) {
        throw std::invalid_argument("decoder and measured_runs must be valid");
    }
    if (reference.size() != input.symbol_count) {
        throw std::invalid_argument("reference size differs from symbol_count");
    }

    std::vector<std::uint32_t> output;
    output.reserve(reference.size());
    for (unsigned i = 0; i < warmup_runs; ++i) {
        decoder(input, tables, output);
        if (output != reference) {
            throw std::runtime_error("decoder differs from reference during warmup");
        }
    }

    std::vector<double> timings_ms;
    timings_ms.reserve(measured_runs);
    for (unsigned i = 0; i < measured_runs; ++i) {
        const auto start = std::chrono::steady_clock::now();
        decoder(input, tables, output);
        const auto end = std::chrono::steady_clock::now();
        if (output != reference) {
            throw std::runtime_error("decoder differs from reference during measurement");
        }
        timings_ms.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }

    std::sort(timings_ms.begin(), timings_ms.end());
    const auto middle = timings_ms.size() / 2;
    const double median_ms = timings_ms.size() % 2 == 0
        ? (timings_ms[middle - 1] + timings_ms[middle]) / 2
        : timings_ms[middle];
    BenchmarkResult result;
    result.median_ms = median_ms;
    if (input.symbol_count != 0 && median_ms > 0) {
        result.ns_per_symbol = median_ms * 1'000'000.0 / input.symbol_count;
        result.million_symbols_per_second = input.symbol_count / (median_ms * 1000.0);
    }
    return result;
}

}  // namespace huffman
