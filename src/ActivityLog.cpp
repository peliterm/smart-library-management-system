// ============================================================================
// ActivityLog.cpp
// Smart Library Management System - CPSC 315
// ============================================================================
#include "ActivityLog.h"

std::string activityName(ActivityType t) {
    switch (t) {
        case ActivityType::CHECKOUT:           return "Borrowed";
        case ActivityType::RETURN:             return "Returned";
        case ActivityType::RESERVE:            return "Reserved";
        case ActivityType::CANCEL_RESERVATION: return "Cancelled reservation";
    }
    return "Unknown";
}

ActivityLog::ActivityLog() : total(0) {}

ActivityLog::~ActivityLog() {
    clear();
}

void ActivityLog::clear() {
    for (auto& pair : tops) {
        Node* cur = pair.second;
        while (cur != nullptr) {
            Node* next = cur->next;
            delete cur;
            cur = next;
        }
    }
    tops.clear();
    depths.clear();
    total = 0;
}

// ---------------------------------------------------------------------------
// push: new node becomes the chain head. The previous head hangs off it, so
// the chain runs newest to oldest and no traversal is needed to insert.
// ---------------------------------------------------------------------------
void ActivityLog::push(int userId, const ActivityEntry& entry) {
    Node* node = new Node(entry);
    auto it = tops.find(userId);
    node->next = (it == tops.end()) ? nullptr : it->second;
    tops[userId] = node;
    depths[userId] += 1;
    total += 1;
}

bool ActivityLog::pop(int userId, ActivityEntry* out) {
    auto it = tops.find(userId);
    if (it == tops.end() || it->second == nullptr) return false;

    Node* node = it->second;
    if (out != nullptr) *out = node->entry;

    it->second = node->next;
    delete node;

    depths[userId] -= 1;
    total -= 1;

    // Drop the account's entry entirely once its chain is empty, so an
    // account that has done nothing is not distinguishable from one that
    // has undone everything.
    if (it->second == nullptr) {
        tops.erase(it);
        depths.erase(userId);
    }
    return true;
}

const ActivityEntry* ActivityLog::peek(int userId) const {
    auto it = tops.find(userId);
    if (it == tops.end() || it->second == nullptr) return nullptr;
    return &it->second->entry;
}

std::vector<ActivityEntry> ActivityLog::recent(int userId, size_t n) const {
    std::vector<ActivityEntry> out;
    auto it = tops.find(userId);
    if (it == tops.end()) return out;

    Node* cur = it->second;
    while (cur != nullptr && out.size() < n) {
        out.push_back(cur->entry);
        cur = cur->next;
    }
    return out;
}

size_t ActivityLog::depth(int userId) const {
    auto it = depths.find(userId);
    return (it == depths.end()) ? 0 : it->second;
}

// ---------------------------------------------------------------------------
// validate: the recorded depth is derived state, so it can drift from the
// chain it describes. This walks every chain and checks both the per-account
// length and the running total against what was recorded.
// ---------------------------------------------------------------------------
bool ActivityLog::validate() const {
    size_t walked = 0;

    for (const auto& pair : tops) {
        size_t len = 0;
        for (Node* cur = pair.second; cur != nullptr; cur = cur->next) {
            ++len;
            if (len > total + 1) return false;      // cycle guard
        }

        auto d = depths.find(pair.first);
        if (d == depths.end() || d->second != len) return false;
        if (len == 0) return false;                 // empty chains are erased
        walked += len;
    }

    if (walked != total) return false;
    if (depths.size() != tops.size()) return false;
    return true;
}
