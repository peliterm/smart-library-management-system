// ============================================================================
// BookInventory.h
// Smart Library Management System - CPSC 315
//
// Hash table keyed on ISBN, using separate chaining for collision resolution.
//
// Design notes:
//   - The hash function is FNV-1a rather than a character sum. ISBN-13 values
//     share long prefixes (every one starts 978 or 979, and books from the
//     same publisher share the registration group and publisher digits), so a
//     function that only accumulates character values clusters records into a
//     narrow band of buckets. FNV-1a mixes each byte into the running hash,
//     so a shared prefix does not produce a shared result.
//   - Chains are singly linked. Average chain length is the load factor, so
//     the table resizes at 0.75 to keep chains short.
//   - Resizing rehashes every record into a new, larger bucket array. This is
//     O(n) but happens rarely because capacity doubles each time.
// ============================================================================
#ifndef BOOK_INVENTORY_H
#define BOOK_INVENTORY_H

#include "Book.h"
#include <string>
#include <vector>
#include <cstdint>

class BookInventory {
public:
    explicit BookInventory(size_t initialBuckets = 16);
    ~BookInventory();

    // Non-copyable: the class owns raw node pointers.
    BookInventory(const BookInventory&) = delete;
    BookInventory& operator=(const BookInventory&) = delete;

    // Inserts a book, or overwrites the record if the ISBN already exists.
    // Returns true if a new record was created, false if one was replaced.
    bool insert(const Book& book);

    // Returns a pointer to the stored record, or nullptr if not found.
    // Pointer is non-owning and valid until the next resize or remove.
    Book* find(const std::string& isbn);

    // Removes a record. Returns false if the ISBN was not present.
    bool remove(const std::string& isbn);

    // Returns copies of every record. Used by the sorting/display layer,
    // which orders IDs rather than the records themselves.
    std::vector<Book> getAll() const;

    size_t size() const { return count; }
    size_t bucketCount() const { return buckets.size(); }
    double loadFactor() const {
        return static_cast<double>(count) / static_cast<double>(buckets.size());
    }

    // Diagnostic: longest chain in the table. Used in testing to confirm the
    // hash function is distributing ISBNs evenly.
    size_t longestChain() const;

private:
    struct Node {
        Book   book;
        Node*  next;
        explicit Node(const Book& b) : book(b), next(nullptr) {}
    };

    std::vector<Node*> buckets;
    size_t count;                       // number of records stored
    static constexpr double MAX_LOAD = 0.75;

    // FNV-1a, 64-bit. Each byte is XORed into the hash, then the hash is
    // multiplied by the FNV prime. The multiply is what spreads a shared
    // prefix across the bucket range.
    static uint64_t hashKey(const std::string& key);

    size_t bucketFor(const std::string& key) const;
    void   resize(size_t newBucketCount);
    void   clear();
};

#endif // BOOK_INVENTORY_H
