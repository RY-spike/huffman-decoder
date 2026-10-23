#include "huffman/benchmark.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition) {
    if (!condition) throw std::runtime_error("benchmark statistics check failed");
}
template <typename Function> void invalid(Function function) {
    try { function(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("invalid timings were accepted");
}
}

int main() {
    try {
        const std::vector<double> samples{4, 1, 3, 2};
        const auto stats = huffman::summarize_timings(samples, 1000);
        require(stats.samples_ms == samples);
        require(stats.median_ms == 2.5 && stats.p25_ms == 1.75 && stats.p75_ms == 3.25);
        require(stats.min_ms == 1 && stats.max_ms == 4);
        require(stats.ns_per_symbol == 2500);
        require(std::abs(stats.million_symbols_per_second - 0.4) < 1e-12);
        const auto empty_input = huffman::summarize_timings({0.1}, 0);
        require(empty_input.ns_per_symbol == 0 && empty_input.million_symbols_per_second == 0);
        invalid([] { huffman::summarize_timings({}, 1); });
        invalid([] { huffman::summarize_timings({-1}, 1); });
        invalid([] { huffman::summarize_timings({std::numeric_limits<double>::quiet_NaN()}, 1); });
        std::cout << "Benchmark statistics checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
