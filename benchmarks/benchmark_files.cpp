#include "huffman/benchmark.hpp"
#include "huffman/encoder.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <locale>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string csv(const std::string& value) {
    std::string result = "\"";
    for (const char c : value) {
        if (c == '"') result += '"';
        result += c;
    }
    return result + '"';
}

std::ofstream open_csv(const fs::path& path) {
    std::ofstream stream(path, std::ios::binary);
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(9);
    return stream;
}

std::vector<std::uint8_t> read_bytes(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot open input: " + path.u8string());
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),
                                    std::istreambuf_iterator<char>()};
    if (stream.bad()) throw std::runtime_error("failed reading: " + path.u8string());
    return bytes;
}

struct Case {
    const char* variant;
    huffman::Decoder decoder;
    bool restore_bytes;
    std::vector<std::uint32_t> symbols;
    std::vector<std::uint8_t> bytes;
    std::vector<double> samples;
    std::vector<unsigned> orders;

    const char* scope() const {
        return restore_bytes ? "decode_and_restore_bytes" : "decode_symbols";
    }
};

unsigned parse_count(const fs::path& text, bool allow_zero) {
    const auto value = text.u8string();
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos) {
        throw std::invalid_argument("run counts must be integers");
    }
    const auto parsed = std::stoul(value);
    if (parsed > 100000 || (!allow_zero && parsed == 0)) {
        throw std::invalid_argument("invalid run count (maximum 100000)");
    }
    return static_cast<unsigned>(parsed);
}

void measure_file(unsigned dataset_id, const fs::path& input_path,
                  const fs::path& output_dir, unsigned warmup, unsigned runs,
                  std::ofstream& datasets, std::ofstream& raw,
                  std::ofstream& summary, std::ofstream& verification) {
    const auto input = read_bytes(input_path);
    const auto encoded = huffman::encode_bytes(input);
    const auto tables = huffman::make_decode_tables(encoded.lengths);
    const std::vector<std::uint32_t> expected(input.begin(), input.end());
    std::array<Case, 6> cases{{
        {"baseline", huffman::decode_baseline, false, {}, {}, {}, {}},
        {"base", huffman::decode_base, false, {}, {}, {}, {}},
        {"buffer", huffman::decode_buffer, false, {}, {}, {}, {}},
        {"baseline", huffman::decode_baseline, true, {}, {}, {}, {}},
        {"base", huffman::decode_base, true, {}, {}, {}, {}},
        {"buffer", huffman::decode_buffer, true, {}, {}, {}, {}},
    }};
    for (auto& item : cases) {
        item.symbols.reserve(expected.size());
        if (item.restore_bytes) item.bytes.resize(input.size());
        item.samples.reserve(runs);
        item.orders.reserve(runs);
    }

    // Table construction, allocation and comparison are outside the timed region.
    const auto execute = [&](Case& item) {
        const auto start = std::chrono::steady_clock::now();
        item.decoder(encoded.data, tables, item.symbols);
        if (item.restore_bytes) {
            if (item.symbols.size() != item.bytes.size()) {
                throw std::runtime_error("decoded size changed before byte restoration");
            }
            for (std::size_t i = 0; i < item.symbols.size(); ++i) {
                item.bytes[i] = static_cast<std::uint8_t>(item.symbols[i]);
            }
        }
        const auto finish = std::chrono::steady_clock::now();
        if (item.symbols != expected || (item.restore_bytes && item.bytes != input)) {
            throw std::runtime_error(std::string("round trip mismatch: ") + item.variant);
        }
        return std::chrono::duration<double, std::milli>(finish - start).count();
    };

    for (unsigned round = 0; round < warmup; ++round) {
        for (std::size_t order = 0; order < cases.size(); ++order) {
            execute(cases[(round + order) % cases.size()]);
        }
    }
    // Rotate the first measured case each round to spread order-related drift.
    for (unsigned round = 0; round < runs; ++round) {
        for (std::size_t order = 0; order < cases.size(); ++order) {
            auto& item = cases[(round + order) % cases.size()];
            item.samples.push_back(execute(item));
            item.orders.push_back(static_cast<unsigned>(order + 1));
        }
    }

    const double average_length = input.empty() ? 0.0
        : static_cast<double>(encoded.data.bit_count) / input.size();
    const double payload_ratio = input.empty() ? 0.0
        : static_cast<double>(encoded.data.bytes.size()) / input.size();
    datasets << dataset_id << ',' << csv(input_path.filename().u8string()) << ','
             << csv(input_path.generic_u8string()) << ',' << input.size() << ','
             << encoded.lengths.size() << ',' << tables.max_length << ','
             << encoded.data.bit_count << ',' << encoded.data.bytes.size() << ','
             << average_length << ',' << payload_ratio << '\n';

    std::array<huffman::BenchmarkResult, 6> statistics;
    for (std::size_t i = 0; i < cases.size(); ++i) {
        statistics[i] = huffman::summarize_timings(cases[i].samples, input.size());
    }
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const auto& item = cases[i];
        const auto& result = statistics[i];
        const double baseline_ms = statistics[item.restore_bytes ? 3 : 0].median_ms;
        const double speedup = result.median_ms > 0 ? baseline_ms / result.median_ms : 0;
        const double mib_per_second = result.median_ms > 0
            ? static_cast<double>(input.size()) * 1000.0 / (1048576.0 * result.median_ms) : 0;
        for (std::size_t round = 0; round < item.samples.size(); ++round) {
            raw << dataset_id << ',' << item.scope() << ',' << item.variant << ','
                << round + 1 << ',' << item.orders[round] << ',' << item.samples[round] << '\n';
        }
        summary << dataset_id << ',' << item.scope() << ',' << item.variant << ','
                << warmup << ',' << runs << ',' << result.median_ms << ','
                << result.p25_ms << ',' << result.p75_ms << ',' << result.min_ms << ','
                << result.max_ms << ',' << result.ns_per_symbol << ','
                << result.million_symbols_per_second << ',' << mib_per_second << ','
                << speedup << '\n';
        std::string restored_file;
        if (item.restore_bytes) {
            restored_file = "restored/" + std::to_string(dataset_id) + "." + item.variant + ".bin";
            std::ofstream restored(output_dir / fs::u8path(restored_file), std::ios::binary);
            restored.exceptions(std::ios::failbit | std::ios::badbit);
            if (!item.bytes.empty()) {
                restored.write(reinterpret_cast<const char*>(item.bytes.data()),
                               static_cast<std::streamsize>(item.bytes.size()));
            }
            restored.close();
        }
        verification << dataset_id << ',' << item.scope() << ',' << item.variant
                     << ",PASS," << warmup + runs << ',' << input.size() << ','
                     << csv(restored_file) << '\n';
    }
    std::cout << "PASS " << input_path.filename().u8string() << " (" << input.size()
              << " bytes, max length " << tables.max_length << ")\n";
}

int run(const std::vector<fs::path>& args) {
    fs::path output_dir;
    std::vector<fs::path> inputs;
    unsigned warmup = 3;
    unsigned runs = 30;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const auto option = args[i].u8string();
        if (option == "--help") {
            std::cout << "benchmark_files --input FILE [--input FILE ...] --output-dir NEW_DIR "
                         "[--warmup 3] [--runs 30]\n";
            return 0;
        }
        if (i + 1 == args.size()) throw std::invalid_argument("missing option value");
        if (option == "--input") inputs.push_back(args[++i]);
        else if (option == "--output-dir") output_dir = args[++i];
        else if (option == "--warmup") warmup = parse_count(args[++i], true);
        else if (option == "--runs") runs = parse_count(args[++i], false);
        else throw std::invalid_argument("unknown option: " + option);
    }
    if (output_dir.empty() || inputs.empty()) {
        throw std::invalid_argument("--input and --output-dir are required");
    }
    if (fs::exists(output_dir)) {
        throw std::invalid_argument("output directory already exists; choose a new run directory");
    }
    for (const auto& input : inputs) {
        if (!fs::is_regular_file(input)) throw std::invalid_argument("input is not a file");
    }
    fs::create_directories(output_dir / "restored");
    auto status = open_csv(output_dir / "status.txt");
    status << "RUNNING\n";
    status.flush();
    try {
        auto datasets = open_csv(output_dir / "datasets.csv");
        auto raw = open_csv(output_dir / "measurements.csv");
        auto summary = open_csv(output_dir / "summary.csv");
        auto verification = open_csv(output_dir / "verification.csv");
        datasets << "dataset_id,name,source_path,input_bytes,distinct_symbols,max_code_length,"
                    "encoded_bits,payload_bytes,average_bits_per_symbol,payload_ratio\n";
        raw << "dataset_id,scope,variant,round,order,elapsed_ms\n";
        summary << "dataset_id,scope,variant,warmup_runs,measured_runs,median_ms,p25_ms,p75_ms,"
                   "min_ms,max_ms,ns_per_symbol,million_symbols_per_second,mib_per_second,speedup\n";
        verification << "dataset_id,scope,variant,status,verified_runs,input_bytes,restored_file\n";
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            measure_file(static_cast<unsigned>(i + 1), inputs[i], output_dir, warmup, runs,
                         datasets, raw, summary, verification);
        }
        datasets.close(); raw.close(); summary.close(); verification.close();
        auto info = open_csv(output_dir / "run-info.txt");
        info << "configuration=" << HUFFMAN_BUILD_CONFIG << "\n";
#ifdef _MSC_VER
        info << "compiler=MSVC " << _MSC_FULL_VER << "\n";
#else
        info << "compiler=" << __VERSION__ << "\n";
#endif
        info << "cplusplus=" << 201703 << "\nclock=steady_clock\n"
             << "clock_is_steady=" << std::chrono::steady_clock::is_steady << "\n"
             << "case_order=rotated_each_round\nallocation=before_timing\n"
             << "comparison=after_each_timed_call\nfile_io=outside_timing\n"
             << "warmup_runs=" << warmup << "\nmeasured_runs=" << runs << '\n';
        info.close();
        status << "PASS\n";
        status.close();
        return 0;
    } catch (const std::exception& error) {
        status << "FAILED: " << error.what() << '\n';
        status.close();
        throw;
    }
}

int checked_run(const std::vector<fs::path>& args) {
    try { return run(args); }
    catch (const std::exception& error) {
        std::cerr << "benchmark_files: " << error.what() << '\n';
        return 1;
    }
}

}  // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::vector<fs::path> args;
    for (int i = 0; i < argc; ++i) args.emplace_back(argv[i]);
    return checked_run(args);
}
#else
int main(int argc, char** argv) {
    std::vector<fs::path> args;
    for (int i = 0; i < argc; ++i) args.push_back(fs::u8path(argv[i]));
    return checked_run(args);
}
#endif
