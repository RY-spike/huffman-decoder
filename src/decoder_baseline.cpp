#include "huffman/decoder.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace huffman {
namespace {

struct DecodedOne {
    std::uint32_t symbol;
    unsigned length;
};

void validate_input(const EncodedData& input, const DecodeTables& tables) {
    const auto required_bytes = input.bit_count / 8 + (input.bit_count % 8 != 0);
    if (required_bytes > input.bytes.size()) {
        throw std::invalid_argument("bit_count exceeds the available bytes");
    }
    if ((input.bit_count == 0) != (input.symbol_count == 0)) {
        throw std::invalid_argument("bit_count and symbol_count disagree");
    }
    if (input.bit_count != 0 && tables.symbols.empty()) {
        throw std::invalid_argument("nonempty input requires a codebook");
    }
}

// Load 64 bits beginning at an arbitrary bit position, zero padding absent bytes.
// Bytes and codewords are both MSB first.
std::uint64_t load_word(const EncodedData& input, std::uint64_t bit_pos) {
    const auto byte_pos = static_cast<std::size_t>(bit_pos / 8);
    const unsigned offset = static_cast<unsigned>(bit_pos % 8);
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        const std::size_t at = byte_pos + i;
        value = (value << 8) | (at < input.bytes.size() ? input.bytes[at] : 0);
    }
    if (offset != 0) {
        const std::size_t at = byte_pos + 8;
        const std::uint64_t next = at < input.bytes.size() ? input.bytes[at] : 0;
        value = (value << offset) | (next >> (8 - offset));
    }
    return value;
}

DecodedOne decode_one(std::uint64_t word, std::uint64_t available,
                      const DecodeTables& tables, bool merged_base) {
    const auto prefix = static_cast<std::size_t>(word >> (64 - kStartBits));
    const unsigned first_length = tables.start[prefix];
    if (first_length == 0) {
        throw std::invalid_argument("prefix is absent from the codebook");
    }
    for (unsigned length = first_length; length <= tables.max_length; ++length) {
        if (length > available) {
            throw std::invalid_argument("truncated codeword at the end of the bitstream");
        }
        if (tables.count[length] == 0) {
            continue;
        }
        const std::uint64_t codeword = word >> (64 - length);
        if (codeword < tables.base_cwd[length] || codeword > tables.limit[length]) {
            continue;
        }
        const auto index = merged_base
            ? static_cast<std::size_t>(static_cast<std::int64_t>(codeword) + tables.base[length])
            : static_cast<std::size_t>(tables.base_sym[length] + codeword - tables.base_cwd[length]);
        return {tables.symbols[index], length};
    }
    throw std::invalid_argument("bitstream contains an invalid codeword");
}

void check_output_count(const EncodedData& input,
                        const std::vector<std::uint32_t>& output) {
    if (output.size() != input.symbol_count) {
        throw std::invalid_argument("decoded symbol count differs from symbol_count");
    }
}

void decode_individual(const EncodedData& input, const DecodeTables& tables,
                       std::vector<std::uint32_t>& output, bool merged_base) {
    validate_input(input, tables);
    output.clear();
    std::uint64_t pos = 0;
    while (pos < input.bit_count) {
        const auto one = decode_one(load_word(input, pos), input.bit_count - pos,
                                    tables, merged_base);
        output.push_back(one.symbol);
        if (output.size() > input.symbol_count) {
            throw std::invalid_argument("too many decoded symbols");
        }
        pos += one.length;
    }
    check_output_count(input, output);
}

}  // namespace

DecodeTables make_decode_tables(const std::vector<SymbolLength>& lengths) {
    DecodeTables tables;
    if (lengths.empty()) {
        return tables;
    }

    std::unordered_set<std::uint32_t> seen;
    std::vector<SymbolLength> ordered = lengths;
    for (const auto& item : ordered) {
        if (item.bit_length == 0 || item.bit_length > kMaxCodeLength) {
            throw std::invalid_argument("code length must be between 1 and 32");
        }
        if (!seen.insert(item.symbol_id).second) {
            throw std::invalid_argument("duplicate symbol ID");
        }
        ++tables.count[item.bit_length];
        tables.max_length = std::max(tables.max_length,
                                     static_cast<unsigned>(item.bit_length));
    }
    if (ordered.size() == 1 && ordered.front().bit_length != 1) {
        throw std::invalid_argument("a single symbol must have code length 1");
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
        if (a.bit_length != b.bit_length) {
            return a.bit_length < b.bit_length;
        }
        return a.symbol_id < b.symbol_id;
    });

    std::uint64_t next_code = 0;
    std::uint64_t next_symbol = 0;
    for (unsigned length = 1; length <= tables.max_length; ++length) {
        next_code = (next_code + tables.count[length - 1]) << 1;
        if (next_code + tables.count[length] > (std::uint64_t{1} << length)) {
            throw std::invalid_argument("code lengths do not form a prefix code");
        }
        tables.base_cwd[length] = next_code;
        tables.base_sym[length] = next_symbol;
        tables.base[length] = static_cast<std::int64_t>(next_symbol)
                            - static_cast<std::int64_t>(next_code);
        if (tables.count[length] != 0) {
            tables.limit[length] = next_code + tables.count[length] - 1;
        }
        next_symbol += tables.count[length];
    }

    tables.symbols.reserve(ordered.size());
    for (const auto& item : ordered) {
        const unsigned length = item.bit_length;
        const std::uint64_t offset = tables.symbols.size() - tables.base_sym[length];
        const std::uint64_t codeword = tables.base_cwd[length] + offset;
        tables.symbols.push_back(item.symbol_id);

        if (length <= kStartBits) {
            const auto begin = static_cast<std::size_t>(codeword << (kStartBits - length));
            const auto end = begin + (std::size_t{1} << (kStartBits - length));
            for (auto prefix = begin; prefix < end; ++prefix) {
                tables.start[prefix] = static_cast<std::uint8_t>(length);
            }
        } else {
            const auto prefix = static_cast<std::size_t>(codeword >> (length - kStartBits));
            if (tables.start[prefix] == 0 || length < tables.start[prefix]) {
                tables.start[prefix] = static_cast<std::uint8_t>(length);
            }
        }
    }
    return tables;
}

void decode_baseline(const EncodedData& input, const DecodeTables& tables,
                     std::vector<std::uint32_t>& output) {
    decode_individual(input, tables, output, false);
}

void decode_base(const EncodedData& input, const DecodeTables& tables,
                 std::vector<std::uint32_t>& output) {
    decode_individual(input, tables, output, true);
}

void decode_buffer(const EncodedData& input, const DecodeTables& tables,
                   std::vector<std::uint32_t>& output) {
    validate_input(input, tables);
    output.clear();
    if (input.bit_count == 0) {
        return;
    }

    const unsigned batch_size = 64 / tables.max_length;
    std::uint64_t pos = 0;
    while (pos < input.bit_count) {
        std::uint64_t word = load_word(input, pos);
        unsigned used = 0;
        for (unsigned i = 0; i < batch_size && pos < input.bit_count; ++i) {
            const std::uint64_t available =
                std::min<std::uint64_t>(64 - used, input.bit_count - pos);
            const auto one = decode_one(word, available, tables, true);
            output.push_back(one.symbol);
            if (output.size() > input.symbol_count) {
                throw std::invalid_argument("too many decoded symbols");
            }
            pos += one.length;
            used += one.length;
            word <<= one.length;
        }
    }
    check_output_count(input, output);
}

}  // namespace huffman
