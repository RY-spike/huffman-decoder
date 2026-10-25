#include "huffman/encoder.hpp"
#include "huffman/decoder.hpp"

#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

// A fixed, manually checkable teaching example; not a performance benchmark.
int main() {
    try {
        const std::vector<std::uint8_t> input{'A', 'B', 'C', 'A'};
        const auto encoded = huffman::encode_bytes(input);
        if (encoded.data.bytes != std::vector<std::uint8_t>{0x58} ||
            encoded.data.bit_count != 6 || encoded.data.symbol_count != 4) {
            throw std::runtime_error("ABCA does not match the manual golden encoding");
        }
        std::cout << "Input: ABCA (4 bytes)\n"
                  << "Manual canonical codebook: A=0, B=10, C=11\n"
                  << "Effective bits: 010110; padding: 00\n"
                  << "Encoded byte: 0x" << std::hex << std::uppercase
                  << static_cast<unsigned>(encoded.data.bytes.front()) << std::dec << '\n'
                  << "bit_count=" << encoded.data.bit_count
                  << "; symbol_count=" << encoded.data.symbol_count << '\n';
        const auto tables = huffman::make_decode_tables(encoded.lengths);
        const std::vector<std::uint32_t> expected(input.begin(), input.end());
        struct Variant {
            const char* name;
            void (*decode)(const huffman::EncodedData&, const huffman::DecodeTables&,
                           std::vector<std::uint32_t>&);
        };
        const Variant variants[] = {
            {"baseline", huffman::decode_baseline},
            {"base", huffman::decode_base},
            {"buffer", huffman::decode_buffer}
        };
        for (const auto& variant : variants) {
            std::vector<std::uint32_t> output;
            variant.decode(encoded.data, tables, output);
            if (output != expected) throw std::runtime_error("ABCA decode mismatch");
            std::cout << variant.name << ": ";
            for (const auto symbol : output) std::cout << static_cast<char>(symbol);
            std::cout << " [PASS]\n";
        }
        std::cout << "DEMO PASS: all three outputs match the original bytes.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "DEMO FAILED: " << error.what() << '\n';
        return 1;
    }
}
