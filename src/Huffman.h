// Huffman.h - Huffman coding over a catalog export.
//
// Build: put every symbol in a min-heap keyed on frequency, repeatedly merge
// the two least frequent, until one node remains. Root-to-leaf paths are the
// codes, so no code prefixes another and the stream needs no separators.
//
// Ties break on the smallest symbol in the subtree, then subtree size.
// Without a rule the heap order decides and two runs could differ, which
// would make the output untestable.

#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <cstddef>

namespace huffman {

// A compressed payload plus everything needed to rebuild the decoding tree.
//
// The frequency table travels with the data rather than the code table. It is
// smaller, and rebuilding the tree from frequencies regenerates exactly the
// same codes, so the two sides cannot disagree about an encoding.
struct Archive {
    std::map<char, uint32_t>   frequencies;
    std::vector<unsigned char> bytes;       // packed bits, MSB first
    size_t                     bitCount;    // significant bits in `bytes`
    size_t                     originalBytes;

    Archive() : bitCount(0), originalBytes(0) {}

    // Bytes the archive occupies, counting the frequency table it carries.
    size_t storedBytes() const;

    // Compressed size as a fraction of the original. Below 1.0 is a saving.
    double ratio() const;
};

// Builds the tree, encodes, and packs. An empty input produces an empty
// archive that round-trips to an empty string.
Archive encode(const std::string& input);

// Rebuilds the tree from the archive's frequency table and walks the bits.
std::string decode(const Archive& archive);

// The code assigned to each symbol, for inspection and for the report. Longer
// codes on rarer symbols is the property worth showing.
std::map<char, std::string> codeTable(const std::string& input);

// Serializes the catalog to the line-oriented text this module compresses.
// Declared here, defined in LibrarySystem.cpp, because it needs the inventory.
// Kept as plain text on purpose: a format a person can read is a format whose
// compression ratio means something.

}  // namespace huffman

#endif // HUFFMAN_H
