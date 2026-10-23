#include "huffman/benchmark.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace huffman {

BenchmarkResult summarize_timings(const std::vector<double>& samples_ms,
                                  std::uint64_t symbol_count) {
    if (samples_ms.empty()) {
        throw std::invalid_argument("at least one timing sample is required");
    }
    for (const double sample : samples_ms) {
        if (!std::isfinite(sample) || sample < 0) {
            throw std::invalid_argument("timing samples must be finite and nonnegative");
        }
    }
    auto sorted = samples_ms;
    std::sort(sorted.begin(), sorted.end());
    const auto percentile = [&sorted](double fraction) {
        const double position = fraction * (sorted.size() - 1);
        const auto lower = static_cast<std::size_t>(position);
        const auto upper = std::min(lower + 1, sorted.size() - 1);
        return sorted[lower] + (position - lower) * (sorted[upper] - sorted[lower]);
    };
    BenchmarkResult result;
    result.samples_ms = samples_ms;
    result.min_ms = sorted.front();
    result.max_ms = sorted.back();
    result.p25_ms = percentile(0.25);
    result.median_ms = percentile(0.5);
    result.p75_ms = percentile(0.75);
    if (symbol_count != 0 && result.median_ms > 0) {
        result.ns_per_symbol = result.median_ms * 1'000'000.0 / symbol_count;
        result.million_symbols_per_second = symbol_count / (result.median_ms * 1000.0);
    }
    return result;
}

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

    return summarize_timings(timings_ms, input.symbol_count);
}

}  // namespace huffman
