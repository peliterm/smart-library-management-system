// ============================================================================
// LibrarySystem.h
// Smart Library Management System - CPSC 315
//
// Integration layer. The inventory, account, deadline, and recommendation
// modules never touch each other's data; every cross-module operation goes
// through a function here.
//
// Module 5 additions:
//   - Checkout assigns a due date and pushes the loan onto the due-date heap.
//   - Return removes that specific loan from the heap, which is the early
//     return case the heap's index map exists to handle.
//   - Checkout also feeds the co-borrowing graph, so the recommender learns
//     from real borrowing activity rather than a separate import step.
//
// Two rules keep the modules consistent:
//   1. All validation runs before the first write.
//   2. Every operation returns a named reason on failure, not just false, so
//      the interface can tell the user which rule stopped them.
// ============================================================================
#ifndef LIBRARY_SYSTEM_H
#define LIBRARY_SYSTEM_H

#include "BookInventory.h"
#include "UserAccounts.h"
#include "DueDateHeap.h"
#include "Recommender.h"
#include <string>
#include <vector>
#include <unordered_map>

// Outcome of an operation that can fail for more than one reason.
enum class Status {
    OK,
    USER_NOT_FOUND,
    BOOK_NOT_FOUND,
    NO_COPIES_AVAILABLE,
    LOAN_LIMIT_REACHED,
    NOT_CHECKED_OUT
};

std::string statusMessage(Status s);

// One line of an overdue notice.
struct OverdueNotice {
    int         userId;
    std::string userName;
    std::string isbn;
    std::string title;
    int         daysLate;
};

class LibrarySystem {
public:
    LibrarySystem();

    static constexpr size_t MAX_ACTIVE_LOANS = 5;
    static constexpr int    LOAN_PERIOD_DAYS = 21;

    BookInventory& books()       { return inventory; }
    UserAccounts&  accounts()    { return users; }
    DueDateHeap&   deadlines()   { return dueDates; }
    Recommender&   recommender() { return recs; }

    // Current day number. Advancing it is how tests simulate time passing.
    int  today() const { return currentDay; }
    void setToday(int day) { currentDay = day; }

    // Checks out one copy. Validates fully, then writes to every module.
    // On success, outLoanId receives the new loan's ID.
    Status checkout(int userId, const std::string& isbn, int* outLoanId = nullptr);

    // Returns a copy, clears the loan from the account, and removes it from
    // the due-date heap by loan ID rather than searching for it.
    Status returnBook(int userId, const std::string& isbn);

    // Every loan due on or before today, earliest first.
    std::vector<OverdueNotice> overdueNotices();

    // Top recommendations for a user, drawn from the co-borrowing graph and
    // ranked through the score BST.
    std::vector<Recommendation> recommendationsFor(int userId, size_t limit);

    // Sorted views. The sort operates on a vector of ISBNs, not on Book
    // objects, so only small keys move during the sort rather than whole
    // records. Sorting is stable, which lets a caller sort by title and then
    // by author to get titles ordered inside each author group.
    std::vector<std::string> isbnsSortedByTitle();
    std::vector<std::string> isbnsSortedByAuthor();

    // Runs every module's own validity check. One call so the test driver
    // can assert system-wide consistency after any operation.
    bool validateAll() const;

private:
    BookInventory inventory;
    UserAccounts  users;
    DueDateHeap   dueDates;
    Recommender   recs;

    int currentDay;
    int nextLoanId;

    // Maps (userId, isbn) to the loan ID, so a return can find the heap
    // entry without scanning. A user cannot hold two copies of one title.
    std::unordered_map<std::string, int> loanIdFor;
    static std::string loanKey(int userId, const std::string& isbn);
};

#endif // LIBRARY_SYSTEM_H
