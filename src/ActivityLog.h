// ============================================================================
// ActivityLog.h
// Smart Library Management System - CPSC 315
//
// A stack of activity entries per account, which is the structure the project
// specification calls for to track user activity.
//
// Why a stack rather than a list. The only question this structure has to
// answer quickly is "what did this patron just do," and the only write it has
// to support is "undo that." Both of those are last-in-first-out, so the
// restriction a stack imposes is not a cost here. It is the point: a stack
// cannot be asked to remove an entry from the middle, which means an undo
// cannot silently skip over an action and reverse an older one instead.
//
// The stack is hand-written over linked nodes rather than wrapping
// std::stack, for the same reason the inventory, account list, heap, and
// recommendation tree are hand-written. Each account owns one chain; the
// chain head is the most recent action.
// ============================================================================
#ifndef ACTIVITY_LOG_H
#define ACTIVITY_LOG_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>

enum class ActivityType {
    CHECKOUT,
    RETURN,
    RESERVE,
    CANCEL_RESERVATION
};

std::string activityName(ActivityType t);

struct ActivityEntry {
    ActivityType type;
    std::string  isbn;
    int          day;        // system day number the action happened on

    ActivityEntry() : type(ActivityType::CHECKOUT), day(0) {}
    ActivityEntry(ActivityType t, const std::string& isbn_, int day_)
        : type(t), isbn(isbn_), day(day_) {}
};

class ActivityLog {
public:
    ActivityLog();
    ~ActivityLog();

    // Non-copyable: the class owns raw node pointers.
    ActivityLog(const ActivityLog&) = delete;
    ActivityLog& operator=(const ActivityLog&) = delete;

    // Push an action onto this account's stack. O(1).
    void push(int userId, const ActivityEntry& entry);

    // Remove and return the most recent action. False when the stack is
    // empty. O(1).
    bool pop(int userId, ActivityEntry* out);

    // Look at the most recent action without removing it. Returns nullptr on
    // an empty stack. O(1).
    const ActivityEntry* peek(int userId) const;

    // Up to n most recent actions, newest first. Walking a stack without
    // destroying it is a read of the chain, not a sequence of pops, so the
    // caller cannot accidentally consume the history by displaying it.
    std::vector<ActivityEntry> recent(int userId, size_t n) const;

    size_t depth(int userId) const;
    size_t totalEntries() const { return total; }

    // Every chain's walked length matches its recorded depth, and the
    // recorded depths sum to the total. Follows the same pattern as the
    // other structures' validity checks.
    bool validate() const;

private:
    struct Node {
        ActivityEntry entry;
        Node*         next;
        explicit Node(const ActivityEntry& e) : entry(e), next(nullptr) {}
    };

    std::unordered_map<int, Node*>  tops;     // userId -> most recent action
    std::unordered_map<int, size_t> depths;   // userId -> chain length
    size_t total;

    void clear();
};

#endif // ACTIVITY_LOG_H
