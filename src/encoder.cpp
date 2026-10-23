#include "huffman/encoder.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <queue>
#include <stdexcept>

namespace huffman {
namespace {

constexpr unsigned kByteAlphabetSize = 256;
constexpr unsigned kEncoderMaxCodeLength = 32;

struct Node {
    std::uint64_t frequency;
    int symbol;
    int left;
    int right;
    unsigned minimum_symbol;
};

struct Code {
    std::uint64_t bits = 0;
    unsigned length = 0;
};

std::vector<SymbolLength> make_lengths(const std::vector<std::uint8_t>& input) {
    std::array<std::uint64_t, kByteAlphabetSize> frequencies{};
    for (const auto symbol : input) {
        ++frequencies[symbol];
    }
    std::vector<Node> nodes;
    const auto compare = [&nodes](int a, int b) {
        const auto& x = nodes[a];
        const auto& y = nodes[b];
        return x.frequency != y.frequency ? x.frequency > y.frequency
                                         : x.minimum_symbol > y.minimum_symbol;
    };
    std::priority_queue<int, std::vector<int>, decltype(compare)> queue(compare);
    for (unsigned symbol = 0; symbol < kByteAlphabetSize; ++symbol) {
        if (frequencies[symbol] != 0) {
            nodes.push_back({frequencies[symbol], static_cast<int>(symbol),
                             -1, -1, symbol});
            queue.push(static_cast<int>(nodes.size() - 1));
        }
    }
    if (queue.empty()) {
        return {};
    }
    while (queue.size() > 1) {
        const int left = queue.top();
        queue.pop();
        const int right = queue.top();
        queue.pop();
        nodes.push_back({nodes[left].frequency + nodes[right].frequency, -1,
                         left, right,
                         std::min(nodes[left].minimum_symbol, nodes[right].minimum_symbol)});
        queue.push(static_cast<int>(nodes.size() - 1));
    }
    std::vector<SymbolLength> lengths;
    const auto visit = [&](auto&& self, int index, unsigned depth) -> void {
        const auto& node = nodes[index];
        if (node.symbol >= 0) {
            const unsigned length = std::max(1u, depth);
            if (length > kEncoderMaxCodeLength) {
                throw std::invalid_argument("Huffman code length exceeds 32 bits");
            }
            lengths.push_back({static_cast<std::uint32_t>(node.symbol),
                               static_cast<std::uint8_t>(length)});
            return;
        }
        self(self, node.left, depth + 1);
        self(self, node.right, depth + 1);
    };
    visit(visit, queue.top(), 0);
    std::sort(lengths.begin(), lengths.end(), [](const auto& a, const auto& b) {
        return a.symbol_id < b.symbol_id;
    });
    return lengths;
}

}  // namespace

EncodeResult encode_bytes(const std::vector<std::uint8_t>& input) {
    EncodeResult result;
    result.lengths = make_lengths(input);
    auto ordered = result.lengths;
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
        return a.bit_length != b.bit_length ? a.bit_length < b.bit_length
                                            : a.symbol_id < b.symbol_id;
    });
    std::array<Code, kByteAlphabetSize> codes{};
    std::uint64_t next = 0;
    unsigned previous_length = 0;
    for (const auto& item : ordered) {
        next <<= item.bit_length - previous_length;
        codes[item.symbol_id] = {next++, item.bit_length};
        previous_length = item.bit_length;
    }

    result.data.symbol_count = input.size();
    std::uint8_t accumulator = 0;
    unsigned used = 0;
    for (const auto symbol : input) {
        const auto& code = codes[symbol];
        if (result.data.bit_count > std::numeric_limits<std::uint64_t>::max() - code.length) {
            throw std::overflow_error("encoded bit count exceeds uint64_t");
        }
        result.data.bit_count += code.length;
        for (unsigned remaining = code.length; remaining > 0; --remaining) {
            accumulator = static_cast<std::uint8_t>(
                (accumulator << 1) | ((code.bits >> (remaining - 1)) & 1u));
            if (++used == 8) {
                result.data.bytes.push_back(accumulator);
                accumulator = 0;
                used = 0;
            }
        }
    }
    if (used != 0) {
        result.data.bytes.push_back(static_cast<std::uint8_t>(accumulator << (8 - used)));
    }
    return result;
}

}  // namespace huffman
