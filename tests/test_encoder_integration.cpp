#include "huffman/encoder.hpp"
#include "huffman/decoder.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

huffman::EncodeResult check_all(const std::vector<std::uint8_t>& input) {
    const auto encoded = huffman::encode_bytes(input);
    require(encoded.data.symbol_count == input.size(), "wrong symbol count");
    require(encoded.data.bytes.size() == (encoded.data.bit_count + 7) / 8,
            "wrong packed byte count");
    if (encoded.data.bit_count % 8 != 0) {
        const unsigned padding = 8 - static_cast<unsigned>(encoded.data.bit_count % 8);
        require((encoded.data.bytes.back() & ((1u << padding) - 1)) == 0,
                "nonzero padding bits");
    }
    const auto tables = huffman::make_decode_tables(encoded.lengths);
    const std::vector<std::uint32_t> expected(input.begin(), input.end());
    std::vector<std::uint32_t> output{999};
    for (const auto decoder : {huffman::decode_baseline, huffman::decode_base,
                               huffman::decode_buffer}) {
        decoder(encoded.data, tables, output);
        require(output == expected, "encoder/decoder round trip differs from input");
    }
    return encoded;
}

std::vector<std::uint8_t> load(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    require(static_cast<bool>(stream), "cannot open legacy fixture");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::uint64_t read_u64(const std::vector<std::uint8_t>& data, std::size_t offset) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) {
        value |= std::uint64_t{data.at(offset + i)} << (8 * i);
    }
    return value;
}

// Optional read-only comparison with the original encoder's HUF1 artifacts.
void check_legacy(const std::string& root) {
    for (const auto* name : {"sample.txt", "tests/empty.bin", "tests/single.bin",
                             "tests/allbytes.bin", "tests/boundary.bin"}) {
        const auto encoded = check_all(load(root + "/" + name));
        const auto file = load(root + "/" + name + ".huf");
        require(file.size() >= 276, "truncated legacy header");
        require(std::string(file.begin(), file.begin() + 4) == "HUF1", "bad legacy magic");
        require(read_u64(file, 4) == encoded.data.symbol_count, "legacy symbol count differs");
        require(read_u64(file, 12) == encoded.data.bit_count, "legacy bit count differs");
        std::vector<std::uint8_t> lengths(256);
        for (const auto& item : encoded.lengths) {
            lengths[item.symbol_id] = item.bit_length;
        }
        require(std::equal(lengths.begin(), lengths.end(), file.begin() + 20),
                "legacy code lengths differ");
        require(std::vector<std::uint8_t>(file.begin() + 276, file.end()) == encoded.data.bytes,
                "legacy payload differs");
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto empty = check_all({});
        require(empty.lengths.empty() && empty.data.bit_count == 0, "wrong empty encoding");
        const auto single = check_all(std::vector<std::uint8_t>(9, 0xFF));
        require(single.lengths.size() == 1 && single.lengths[0].bit_length == 1,
                "single symbol must use length 1");
        require(single.data.bit_count == 9 && single.data.bytes == std::vector<std::uint8_t>({0, 0}),
                "single symbol must encode as zero bits");
        const auto abc = check_all({'A', 'B', 'C', 'A'});
        require(abc.data.bit_count == 6 && abc.data.bytes == std::vector<std::uint8_t>({0x58}),
                "ABCA golden bitstream mismatch");

        std::vector<std::uint8_t> all_bytes;
        for (unsigned i = 0; i < 256; ++i) {
            all_bytes.push_back(static_cast<std::uint8_t>(i));
        }
        const auto uniform = check_all(all_bytes);
        require(uniform.data.bytes == all_bytes && uniform.data.bit_count == 2048,
                "equal-frequency tie breaking changed");
        for (const auto& item : uniform.lengths) {
            require(item.bit_length == 8, "uniform alphabet must use eight-bit codes");
        }
        for (unsigned count = 1; count <= 65; ++count) {
            std::vector<std::uint8_t> boundary;
            for (unsigned i = 0; i < count; ++i) {
                boundary.push_back(static_cast<std::uint8_t>(i % 5));
            }
            check_all(boundary);
        }
        std::vector<std::uint8_t> random;
        std::uint32_t state = 7;
        for (unsigned i = 0; i < 65536; ++i) {
            state = state * 1664525u + 1013904223u;
            random.push_back(static_cast<std::uint8_t>(state >> 24));
        }
        check_all(random);
        std::vector<std::uint8_t> skewed;
        unsigned previous = 1;
        unsigned frequency = 1;
        for (unsigned i = 0; i < 20; ++i) {
            skewed.insert(skewed.end(), frequency, static_cast<std::uint8_t>(i));
            const auto next = previous + frequency;
            previous = frequency;
            frequency = next;
        }
        const auto long_codes = check_all(skewed);
        require(std::any_of(long_codes.lengths.begin(), long_codes.lengths.end(),
                            [](const auto& item) { return item.bit_length > 12; }),
                "long-code decoder path was not exercised");
        if (argc == 2) {
            check_legacy(argv[1]);
        }
        std::cout << "All encoder integration checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
