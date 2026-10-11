// ============================================================================
// BookInventory.cpp
// Smart Library Management System - CPSC 315
// ============================================================================
#include "BookInventory.h"

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
            cur->book = book;   // update in place
            return false;
        }
    }

    // Not present: add to the front of the chain.
    Node* node = new Node(book);
    node->next = buckets[idx];
    buckets[idx] = node;
    ++count;

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
}
