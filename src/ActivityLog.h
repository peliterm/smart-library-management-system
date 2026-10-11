// ActivityLog.h - a stack of actions per account, for display and undo.
//
// A stack rather than a list because both operations are last-in-first-out,
// and the restriction is the point: an undo cannot skip over an action to
// reverse an older one.

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

// Stores actions for undo. Loan IDs distinguish duplicate copies of one
// title; original due dates stop an undo from extending a loan. Both are 0
// for actions with no loan, such as reservations.
struct ActivityEntry {
    ActivityType type;
    std::string  isbn;
    int          day;        // system day number the action happened on
    int          loanId;     // the specific loan this action affected, or 0
    int          dueDay;     // that loan's due date, so undo can restore it

    ActivityEntry()
        : type(ActivityType::CHECKOUT), day(0), loanId(0), dueDay(0) {}

    ActivityEntry(ActivityType t, const std::string& isbn_, int day_,
                  int loanId_ = 0, int dueDay_ = 0)
        : type(t), isbn(isbn_), day(day_), loanId(loanId_), dueDay(dueDay_) {}
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
