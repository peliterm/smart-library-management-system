// ============================================================================
// ReservationQueue.cpp
// Smart Library Management System - CPSC 315
// ============================================================================
#include "ReservationQueue.h"

ReservationQueue::ReservationQueue() : total(0) {}

ReservationQueue::~ReservationQueue() {
    clear();
}

void ReservationQueue::clear() {
    for (auto& pair : lines) {
        Node* cur = pair.second.head;
        while (cur != nullptr) {
            Node* next = cur->next;
            delete cur;
            cur = next;
        }
    }
    lines.clear();
    total = 0;
}

// ---------------------------------------------------------------------------
// enqueue: append at the tail. The duplicate check runs first, because a
// patron who asks twice should keep their original place rather than taking
// two of them.
// ---------------------------------------------------------------------------
bool ReservationQueue::enqueue(const std::string& isbn, int userId) {
    if (isWaiting(isbn, userId)) return false;

    Line& line = lines[isbn];
    Node* node = new Node(userId);

    if (line.tail == nullptr) {
        line.head = node;
        line.tail = node;
    } else {
        line.tail->next = node;
        line.tail = node;
    }
    line.count += 1;
    total += 1;
    return true;
}

// ---------------------------------------------------------------------------
// dequeue: take from the head. When the line empties, the map entry is
// erased so a title nobody is waiting on does not linger as an empty Line.
// ---------------------------------------------------------------------------
bool ReservationQueue::dequeue(const std::string& isbn, int* outUserId) {
    auto it = lines.find(isbn);
    if (it == lines.end() || it->second.head == nullptr) return false;

    Line& line = it->second;
    Node* node = line.head;
    if (outUserId != nullptr) *outUserId = node->userId;

    line.head = node->next;
    if (line.head == nullptr) line.tail = nullptr;   // line is now empty
    delete node;

    line.count -= 1;
    total -= 1;

    if (line.head == nullptr) lines.erase(it);
    return true;
}

// ---------------------------------------------------------------------------
// cancel: remove from anywhere in the line.
//
// The tail pointer is the trap here. Removing the last node without moving
// the tail leaves it dangling at freed memory, and the next enqueue writes
// through it. The previous-node pointer carried through the walk is what
// makes the tail fixable without a second pass.
// ---------------------------------------------------------------------------
bool ReservationQueue::cancel(const std::string& isbn, int userId) {
    auto it = lines.find(isbn);
    if (it == lines.end()) return false;

    Line& line = it->second;
    Node* prev = nullptr;
    Node* cur  = line.head;

    while (cur != nullptr && cur->userId != userId) {
        prev = cur;
        cur  = cur->next;
    }
    if (cur == nullptr) return false;                // not in this line

    if (prev == nullptr) {
        line.head = cur->next;                       // removing the front
    } else {
        prev->next = cur->next;
    }
    if (cur == line.tail) line.tail = prev;          // removing the back

    delete cur;
    line.count -= 1;
    total -= 1;

    if (line.head == nullptr) {
        line.tail = nullptr;
        lines.erase(it);
    }
    return true;
}

bool ReservationQueue::isWaiting(const std::string& isbn, int userId) const {
    auto it = lines.find(isbn);
    if (it == lines.end()) return false;
    for (Node* cur = it->second.head; cur != nullptr; cur = cur->next) {
        if (cur->userId == userId) return true;
    }
    return false;
}

int ReservationQueue::frontOf(const std::string& isbn) const {
    auto it = lines.find(isbn);
    if (it == lines.end() || it->second.head == nullptr) return 0;
    return it->second.head->userId;
}

size_t ReservationQueue::lengthOf(const std::string& isbn) const {
    auto it = lines.find(isbn);
    return (it == lines.end()) ? 0 : it->second.count;
}

std::vector<int> ReservationQueue::waitingOn(const std::string& isbn) const {
    std::vector<int> out;
    auto it = lines.find(isbn);
    if (it == lines.end()) return out;
    for (Node* cur = it->second.head; cur != nullptr; cur = cur->next) {
        out.push_back(cur->userId);
    }
    return out;
}

std::vector<std::string> ReservationQueue::titlesWithHolds() const {
    std::vector<std::string> out;
    out.reserve(lines.size());
    for (const auto& pair : lines) {
        out.push_back(pair.first);
    }
    return out;
}

// ---------------------------------------------------------------------------
// validate: counts are derived state and the tail pointer is a shortcut, so
// both are checked against the chain they describe.
// ---------------------------------------------------------------------------
bool ReservationQueue::validate() const {
    size_t walked = 0;

    for (const auto& pair : lines) {
        const Line& line = pair.second;
        if (line.head == nullptr || line.tail == nullptr) return false;

        size_t len = 0;
        Node*  last = nullptr;
        for (Node* cur = line.head; cur != nullptr; cur = cur->next) {
            ++len;
            if (len > total + 1) return false;       // cycle guard

            // No patron may hold two places in one line.
            for (Node* other = cur->next; other != nullptr; other = other->next) {
                if (other->userId == cur->userId) return false;
            }
            last = cur;
        }

        if (len != line.count) return false;
        if (last != line.tail)  return false;        // tail really is last
        walked += len;
    }

    return walked == total;
}
