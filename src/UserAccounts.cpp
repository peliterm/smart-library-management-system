// ============================================================================
// UserAccounts.cpp
// Smart Library Management System - CPSC 315
// ============================================================================
#include "UserAccounts.h"

UserAccounts::UserAccounts()
    : head(nullptr), tail(nullptr), count(0) {}

UserAccounts::~UserAccounts() {
    clear();
}

// ---------------------------------------------------------------------------
// addUser: places the account in sorted position by user ID.
//
// Three cases, ordered by how often they occur:
//   1. List is empty.
//   2. New ID is greater than the tail's ID (the common case, since IDs are
//      handed out in increasing order). Append at the tail, O(1).
//   3. New ID falls somewhere inside the list. Walk backward from the tail
//      until the insertion point is found, then relink. O(n) worst case.
// ---------------------------------------------------------------------------
bool UserAccounts::addUser(const User& user) {
    // Reject duplicates using the index, which is O(1) rather than a scan.
    if (index.find(user.userId) != index.end()) {
        return false;
    }

    Node* node = new Node(user);

    if (head == nullptr) {                       // case 1: empty list
        head = tail = node;
    } else if (user.userId > tail->user.userId) { // case 2: append
        node->prev = tail;
        tail->next = node;
        tail = node;
    } else {                                      // case 3: interior insert
        Node* cur = tail;
        while (cur != nullptr && cur->user.userId > user.userId) {
            cur = cur->prev;
        }
        if (cur == nullptr) {                     // becomes the new head
            node->next = head;
            head->prev = node;
            head = node;
        } else {                                  // insert after cur
            node->next = cur->next;
            node->prev = cur;
            if (cur->next != nullptr) {
                cur->next->prev = node;
            }
            cur->next = node;
        }
    }

    index[user.userId] = node;   // keep the index in step with the list
    ++count;
    return true;
}

// ---------------------------------------------------------------------------
// removeUser: the index gives the node directly, so no traversal is required.
// ---------------------------------------------------------------------------
bool UserAccounts::removeUser(int userId) {
    auto it = index.find(userId);
    if (it == index.end()) {
        return false;
    }

    Node* node = it->second;

    if (node->prev != nullptr) {
        node->prev->next = node->next;
    } else {
        head = node->next;          // removing the head
    }

    if (node->next != nullptr) {
        node->next->prev = node->prev;
    } else {
        tail = node->prev;          // removing the tail
    }

    delete node;
    index.erase(it);
    --count;
    return true;
}

User* UserAccounts::findUser(int userId) {
    auto it = index.find(userId);
    return (it == index.end()) ? nullptr : &it->second->user;
}

std::vector<User> UserAccounts::listAll() const {
    std::vector<User> out;
    out.reserve(count);
    for (Node* cur = head; cur != nullptr; cur = cur->next) {
        out.push_back(cur->user);
    }
    return out;
}

// ---------------------------------------------------------------------------
// validate: confirms the list is sorted, that node counts agree, and that
// every index entry points at the node actually holding that account.
// Keeping two structures in sync is the main risk of this design, so the
// check exists to catch a desync during testing rather than in use.
// ---------------------------------------------------------------------------
bool UserAccounts::validate() const {
    size_t walked = 0;
    const Node* prev = nullptr;

    for (const Node* cur = head; cur != nullptr; cur = cur->next) {
        if (prev != nullptr && prev->user.userId >= cur->user.userId) {
            return false;                       // ordering broken
        }
        if (cur->prev != prev) {
            return false;                       // back pointer broken
        }
        auto it = index.find(cur->user.userId);
        if (it == index.end() || it->second != cur) {
            return false;                       // index disagrees with list
        }
        prev = cur;
        ++walked;
    }

    if (prev != tail)          return false;    // tail pointer stale
    if (walked != count)       return false;    // count stale
    if (index.size() != count) return false;    // orphaned index entries
    return true;
}

void UserAccounts::clear() {
    Node* cur = head;
    while (cur != nullptr) {
        Node* next = cur->next;
        delete cur;
        cur = next;
    }
    head = tail = nullptr;
    index.clear();
    count = 0;
}
