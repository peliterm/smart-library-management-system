// Huffman.cpp

#include "Huffman.h"
#include <queue>
#include <algorithm>

namespace huffman {

namespace {

struct Node {
    char     symbol;        // meaningful only at a leaf
    uint32_t frequency;
    char     smallestSymbol;   // smallest symbol anywhere in this subtree
    size_t   leafCount;
    Node*    left;
    Node*    right;

    Node(char s, uint32_t f)
        : symbol(s), frequency(f), smallestSymbol(s), leafCount(1),
          left(nullptr), right(nullptr) {}

    Node(Node* l, Node* r)
        : symbol(0), frequency(l->frequency + r->frequency),
          smallestSymbol(std::min(l->smallestSymbol, r->smallestSymbol)),
          leafCount(l->leafCount + r->leafCount), left(l), right(r) {}

    bool isLeaf() const { return left == nullptr && right == nullptr; }
};

// Min-heap ordering. Frequency decides; the tie-breakers exist so the tree is
// a function of the input alone and not of the heap's internal arrangement.
struct Heavier {
    bool operator()(const Node* a, const Node* b) const {
        if (a->frequency != b->frequency) return a->frequency > b->frequency;
        if (a->smallestSymbol != b->smallestSymbol)
            return a->smallestSymbol > b->smallestSymbol;
        return a->leafCount > b->leafCount;
    }
};

void destroy(Node* node) {
    if (node == nullptr) return;
    destroy(node->left);
    destroy(node->right);
    delete node;
}

// Builds the tree from a frequency table. Returns nullptr for an empty table.
Node* buildTree(const std::map<char, uint32_t>& freq) {
    if (freq.empty()) return nullptr;

    std::priority_queue<Node*, std::vector<Node*>, Heavier> heap;
    for (const auto& pair : freq) {
        heap.push(new Node(pair.first, pair.second));
    }

    // A single distinct symbol has no path to a leaf, so it would be assigned
    // a zero-length code and the stream would carry no bits at all. Pairing it
    // with itself under a parent gives it a one-bit code.
    if (heap.size() == 1) {
        Node* only = heap.top();
        heap.pop();
        return new Node(only, new Node(only->symbol, 0));
    }

    while (heap.size() > 1) {
        Node* a = heap.top(); heap.pop();     // two least frequent
        Node* b = heap.top(); heap.pop();
        heap.push(new Node(a, b));            // merged, frequency summed
    }
    Node* root = heap.top();
    heap.pop();
    return root;
}

void collectCodes(const Node* node, const std::string& prefix,
                  std::map<char, std::string>& out) {
    if (node == nullptr) return;
    if (node->isLeaf()) {
        out[node->symbol] = prefix.empty() ? "0" : prefix;
        return;
    }
    collectCodes(node->left,  prefix + "0", out);
    collectCodes(node->right, prefix + "1", out);
}

std::map<char, uint32_t> countFrequencies(const std::string& input) {
    std::map<char, uint32_t> freq;
    for (char c : input) freq[c] += 1;
    return freq;
}

}  // namespace

size_t Archive::storedBytes() const {
    // Payload, plus the frequency table carried with it: one byte for the
    // symbol and four for the count. Reporting the payload alone would
    // overstate the saving by hiding the cost of the header.
    const size_t header = frequencies.size() * (1 + 4);
    return bytes.size() + header;
}

double Archive::ratio() const {
    if (originalBytes == 0) return 0.0;
    return static_cast<double>(storedBytes()) /
           static_cast<double>(originalBytes);
}

std::map<char, std::string> codeTable(const std::string& input) {
    std::map<char, uint32_t> freq = countFrequencies(input);
    Node* root = buildTree(freq);
    std::map<char, std::string> codes;
    collectCodes(root, "", codes);
    destroy(root);
    return codes;
}

Archive encode(const std::string& input) {
    Archive archive;
    archive.originalBytes = input.size();
    if (input.empty()) return archive;

    archive.frequencies = countFrequencies(input);

    Node* root = buildTree(archive.frequencies);
    std::map<char, std::string> codes;
    collectCodes(root, "", codes);
    destroy(root);

    // Pack the bit string into bytes, most significant bit first. The bit
    // count is stored because the final byte is usually partly padding, and
    // without it the decoder would walk into the padding and emit a symbol
    // that was never written.
    unsigned char current = 0;
    int filled = 0;
    for (char c : input) {
        for (char bit : codes[c]) {
            current = static_cast<unsigned char>((current << 1) | (bit == '1' ? 1 : 0));
            ++filled;
            ++archive.bitCount;
            if (filled == 8) {
                archive.bytes.push_back(current);
                current = 0;
                filled = 0;
            }
        }
    }
    if (filled > 0) {
        current = static_cast<unsigned char>(current << (8 - filled));  // pad
        archive.bytes.push_back(current);
    }
    return archive;
}

std::string decode(const Archive& archive) {
    if (archive.bitCount == 0) return std::string();

    Node* root = buildTree(archive.frequencies);
    if (root == nullptr) return std::string();

    std::string out;
    out.reserve(archive.originalBytes);

    const Node* cursor = root;
    for (size_t bit = 0; bit < archive.bitCount; ++bit) {
        const size_t byteIndex = bit / 8;
        const int    bitIndex  = 7 - static_cast<int>(bit % 8);
        const bool   one = ((archive.bytes[byteIndex] >> bitIndex) & 1) != 0;

        cursor = one ? cursor->right : cursor->left;
        if (cursor == nullptr) break;           // malformed archive

        if (cursor->isLeaf()) {
            out += cursor->symbol;
            cursor = root;
        }
    }

    destroy(root);
    return out;
}

}  // namespace huffman
