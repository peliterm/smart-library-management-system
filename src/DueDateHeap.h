// DueDateHeap.h - binary min-heap on due date, plus a map from loan ID to
// array index.
//
// A plain heap cannot remove an arbitrary element without an O(n) scan, and
// early returns are the most frequent write the system has. The index map,
// updated inside every sift, makes removal O(log n).

#ifndef DUE_DATE_HEAP_H
#define DUE_DATE_HEAP_H

#include <string>
#include <vector>
#include <unordered_map>

// A loan waiting to come back. dueDay is a day number rather than a calendar
// date to keep comparison simple and testable.
struct LoanEntry {
    int         loanId;
    int         userId;
    std::string isbn;
    int         dueDay;

    LoanEntry() : loanId(0), userId(0), dueDay(0) {}
    LoanEntry(int loanId_, int userId_, const std::string& isbn_, int dueDay_)
        : loanId(loanId_), userId(userId_), isbn(isbn_), dueDay(dueDay_) {}
};

class DueDateHeap {
public:
    // O(log n). Rejects a duplicate loan ID.
    bool push(const LoanEntry& entry);

    // O(1). Returns the loan with the earliest due date without removing it.
    const LoanEntry* peekMin() const;

    // O(log n). Removes and returns the earliest loan. False if empty.
    bool popMin(LoanEntry& out);

    // O(log n) average. Removes a specific loan, which is the early-return
    // case a plain heap cannot do without a linear scan.
    bool removeLoan(int loanId);

    // O(1) average. Looks up a loan without disturbing the heap.
    const LoanEntry* find(int loanId) const;

    // All loans due on or before the given day, earliest first.
    // Copies the heap so the real one is not drained by generating a report.
    std::vector<LoanEntry> overdueAsOf(int today) const;

    size_t size()  const { return data.size(); }
    bool   empty() const { return data.empty(); }

    // Verification helper in the spirit of the BST validity checker from
    // Lab 7.15. Confirms two invariants at once:
    //   1. Every parent's due day is <= both of its children's.
    //   2. Every index map entry points at the array slot actually holding
    //      that loan ID.
    // The second is the one that matters, since a missed map update during a
    // sift produces a heap that still looks correct but silently breaks
    // removal.
    bool validate() const;

private:
    std::vector<LoanEntry> data;              // the heap array
    std::unordered_map<int, size_t> indexOf;  // loan ID -> array index

    void siftUp(size_t i);
    void siftDown(size_t i);
    void swapNodes(size_t a, size_t b);       // swaps array slots AND the map
};

#endif // DUE_DATE_HEAP_H
