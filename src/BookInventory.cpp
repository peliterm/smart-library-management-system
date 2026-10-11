// BookInventory.cpp

#include "BookInventory.h"
#include <algorithm>
#include <cctype>

BookInventory::BookInventory(size_t initialBuckets)
    : buckets(initialBuckets, nullptr), count(0) {}

BookInventory::~BookInventory() {
    clear();
}

// ---------------------------------------------------------------------------
// FNV-1a hash. Constants are the standard 64-bit offset basis and prime.
// ---------------------------------------------------------------------------
uint64_t BookInventory::hashKey(const std::string& key) {
    const uint64_t FNV_OFFSET = 1469598103934665603ULL;
    const uint64_t FNV_PRIME  = 1099511628211ULL;

    uint64_t hash = FNV_OFFSET;
    for (char c : key) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= FNV_PRIME;
    }
    return hash;
}

size_t BookInventory::bucketFor(const std::string& key) const {
    return static_cast<size_t>(hashKey(key) % buckets.size());
}

// ---------------------------------------------------------------------------
// insert: replaces the record if the ISBN is already present, otherwise
// pushes a new node at the head of the chain (O(1) once the chain is scanned).
// ---------------------------------------------------------------------------
bool BookInventory::insert(const Book& book) {
    size_t idx = bucketFor(book.isbn);

    // Scan the chain for an existing record with this ISBN.
    for (Node* cur = buckets[idx]; cur != nullptr; cur = cur->next) {
        if (cur->book.isbn == book.isbn) {
            deindexRecord(cur->book);   // drop the old title's words first
            cur->book = book;           // update in place
            indexRecord(cur->book);
            return false;
        }
    }

    // Not present: add to the front of the chain.
    Node* node = new Node(book);
    node->next = buckets[idx];
    buckets[idx] = node;
    ++count;
    indexRecord(book);

    // Grow before chains get long enough to hurt lookup time.
    if (loadFactor() > MAX_LOAD) {
        resize(buckets.size() * 2);
    }
    return true;
}

Book* BookInventory::find(const std::string& isbn) {
    size_t idx = bucketFor(isbn);
    for (Node* cur = buckets[idx]; cur != nullptr; cur = cur->next) {
        if (cur->book.isbn == isbn) {
            return &cur->book;
        }
    }
    return nullptr;
}

bool BookInventory::remove(const std::string& isbn) {
    size_t idx = bucketFor(isbn);
    Node* cur  = buckets[idx];
    Node* prev = nullptr;

    while (cur != nullptr) {
        if (cur->book.isbn == isbn) {
            deindexRecord(cur->book);       // keep the word index in step
            if (prev == nullptr) {
                buckets[idx] = cur->next;   // removing the chain head
            } else {
                prev->next = cur->next;
            }
            delete cur;
            --count;
            return true;
        }
        prev = cur;
        cur  = cur->next;
    }
    return false;   // ISBN not found
}

std::vector<Book> BookInventory::getAll() const {
    std::vector<Book> out;
    out.reserve(count);
    for (Node* head : buckets) {
        for (Node* cur = head; cur != nullptr; cur = cur->next) {
            out.push_back(cur->book);
        }
    }
    return out;
}

size_t BookInventory::longestChain() const {
    size_t longest = 0;
    for (Node* head : buckets) {
        size_t len = 0;
        for (Node* cur = head; cur != nullptr; cur = cur->next) ++len;
        if (len > longest) longest = len;
    }
    return longest;
}

// ---------------------------------------------------------------------------
// resize: allocates a larger bucket array and rehashes every record into it.
// Nodes are moved rather than reallocated, so no Book copies are made.
// ---------------------------------------------------------------------------
void BookInventory::resize(size_t newBucketCount) {
    std::vector<Node*> oldBuckets = buckets;
    buckets.assign(newBucketCount, nullptr);

    for (Node* head : oldBuckets) {
        Node* cur = head;
        while (cur != nullptr) {
            Node* next = cur->next;             // save before relinking
            size_t idx = bucketFor(cur->book.isbn);
            cur->next  = buckets[idx];
            buckets[idx] = cur;
            cur = next;
        }
    }
}

void BookInventory::clear() {
    for (Node*& head : buckets) {
        while (head != nullptr) {
            Node* next = head->next;
            delete head;
            head = next;
        }
    }
    count = 0;
    wordIndex.clear();   // the secondary index describes records now gone
}


// ---------------------------------------------------------------------------
// Secondary word index.
//
// tokenize lowercases and splits on anything that is not a letter or digit,
// so "O'Brien" yields "o" and "brien" and "C++" yields "c". Punctuation in a
// title should never decide whether a search finds it.
// ---------------------------------------------------------------------------
std::vector<std::string> BookInventory::tokenize(const std::string& text) {
    std::vector<std::string> words;
    std::string current;
    for (unsigned char c : text) {
        if (std::isalnum(c)) {
            current += static_cast<char>(std::tolower(c));
        } else if (!current.empty()) {
            words.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) words.push_back(current);
    return words;
}

void BookInventory::indexRecord(const Book& book) {
    for (const std::string& word : tokenize(book.title + " " + book.author)) {
        std::vector<std::string>& isbns = wordIndex[word];
        // A word repeated in one title must not list the ISBN twice, or a
        // search would report the same book more than once.
        if (std::find(isbns.begin(), isbns.end(), book.isbn) == isbns.end()) {
            isbns.push_back(book.isbn);
        }
    }
}

void BookInventory::deindexRecord(const Book& book) {
    for (const std::string& word : tokenize(book.title + " " + book.author)) {
        auto it = wordIndex.find(word);
        if (it == wordIndex.end()) continue;

        std::vector<std::string>& isbns = it->second;
        isbns.erase(std::remove(isbns.begin(), isbns.end(), book.isbn),
                    isbns.end());
        if (isbns.empty()) wordIndex.erase(it);   // no empty buckets left behind
    }
}

std::vector<std::string> BookInventory::findByWord(const std::string& word) const {
    std::vector<std::string> lowered = tokenize(word);
    if (lowered.empty()) return std::vector<std::string>();

    auto it = wordIndex.find(lowered.front());
    return (it == wordIndex.end()) ? std::vector<std::string>() : it->second;
}

std::vector<std::string> BookInventory::searchSubstring(
        const std::string& text) const {
    std::string needle;
    for (unsigned char c : text) needle += static_cast<char>(std::tolower(c));

    std::vector<std::string> out;
    for (const Book& b : getAll()) {
        std::string haystack;
        for (unsigned char c : b.title + " " + b.author) {
            haystack += static_cast<char>(std::tolower(c));
        }
        if (haystack.find(needle) != std::string::npos) out.push_back(b.isbn);
    }
    return out;
}

// ---------------------------------------------------------------------------
// search: indexed first, scan only as a fallback.
//
// A multi-word query intersects the posting lists of each word, which is the
// O(m + n) merge described in the Module 2 post. A query that matches no
// whole word falls through to the substring scan, so partial words still
// work even though the index cannot answer them.
// ---------------------------------------------------------------------------
std::vector<std::string> BookInventory::search(const std::string& query) const {
    std::vector<std::string> words = tokenize(query);
    if (words.empty()) return std::vector<std::string>();

    std::vector<std::string> result = findByWord(words.front());
    for (size_t i = 1; i < words.size() && !result.empty(); ++i) {
        std::vector<std::string> next = findByWord(words[i]);
        std::vector<std::string> both;
        for (const std::string& isbn : result) {
            if (std::find(next.begin(), next.end(), isbn) != next.end()) {
                both.push_back(isbn);
            }
        }
        result = both;
    }

    if (result.empty()) return searchSubstring(query);
    return result;
}
