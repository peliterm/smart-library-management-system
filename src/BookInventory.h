// BookInventory.h - hash table on ISBN, separate chaining, FNV-1a.
//
// FNV-1a rather than a character sum: ISBN-13 values share long prefixes
// (978/979, registration group, publisher), and a hash that only accumulates
// character values clusters them into a narrow band of buckets.
//
// A secondary word index maps each lowercased title and author word to the
// ISBNs containing it, so a title search is not a catalog scan. It cannot do
// prefixes; searchSubstring is the fallback for those.

#ifndef BOOK_INVENTORY_H
#define BOOK_INVENTORY_H

#include "Book.h"
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

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

    // Word index. A one-word query is O(1) average plus the bucket's list,
    // instead of a catalog scan. It cannot do prefixes: "apol" is not a token
    // and hashes nowhere near "apollo".
    std::vector<std::string> findByWord(const std::string& word) const;

    // Every ISBN whose title or author contains the text anywhere, including
    // mid-word. O(n), and used only when the indexed lookup finds nothing.
    std::vector<std::string> searchSubstring(const std::string& text) const;

    // Indexed lookup first, substring scan only if that finds nothing, so the
    // common case is fast and the partial-word case still works.
    std::vector<std::string> search(const std::string& query) const;

    size_t indexedWordCount() const { return wordIndex.size(); }

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

    // word -> ISBNs containing it in title or author. Kept in step with the
    // main table by insert and remove; a stale entry here would surface a
    // book that is no longer in the catalog.
    std::unordered_map<std::string, std::vector<std::string>> wordIndex;

    static std::vector<std::string> tokenize(const std::string& text);
    void indexRecord(const Book& book);
    void deindexRecord(const Book& book);

    // FNV-1a, 64-bit. Each byte is XORed into the hash, then the hash is
    // multiplied by the FNV prime. The multiply is what spreads a shared
    // prefix across the bucket range.
    static uint64_t hashKey(const std::string& key);

    size_t bucketFor(const std::string& key) const;
    void   resize(size_t newBucketCount);
    void   clear();
};

#endif // BOOK_INVENTORY_H
