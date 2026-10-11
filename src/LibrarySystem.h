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
#include "ActivityLog.h"
#include "ReservationQueue.h"
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
    NOT_CHECKED_OUT,
    // Module 9 additions, for the reservation and undo paths.
    ALREADY_RESERVED,
    NOT_RESERVED,
    COPY_IS_AVAILABLE,     // reserving is pointless; borrow it instead
    NOTHING_TO_UNDO,
    UNDO_NOT_POSSIBLE      // the action could not be reversed from here
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

    BookInventory&    books()        { return inventory; }
    UserAccounts&     accounts()     { return users; }
    DueDateHeap&      deadlines()    { return dueDates; }
    Recommender&      recommender()  { return recs; }
    ActivityLog&      activity()     { return log; }
    ReservationQueue& reservations() { return holds; }

    // Current day number. Advancing it is how tests simulate time passing.
    int  today() const { return currentDay; }
    void setToday(int day) { currentDay = day; }

    // Checks out one copy. Validates fully, then writes to every module.
    // On success, outLoanId receives the new loan's ID.
    Status checkout(int userId, const std::string& isbn, int* outLoanId = nullptr);

    // Returns a copy, clears the loan from the account, and removes it from
    // the due-date heap by loan ID rather than searching for it.
    //
    // If anyone is waiting on the title, the returned copy is handed to the
    // patron at the front of the reservation queue rather than going back on
    // the shelf. When that happens and outFilledFor is supplied, it receives
    // that patron's ID so the interface can say who got it.
    Status returnBook(int userId, const std::string& isbn,
                      int* outFilledFor = nullptr);

    // ---- Module 9: reservations (queue) ----

    // Join the hold line for a title. Refused when a copy is on the shelf,
    // because the patron should simply borrow it, and when this patron is
    // already in the line.
    Status reserve(int userId, const std::string& isbn);

    // Leave the hold line.
    Status cancelReservation(int userId, const std::string& isbn);

    // ---- Module 9: activity log (stack) ----

    // Reverse this account's most recent action. Pops the activity stack and
    // applies the inverse operation. If the inverse fails, the entry is
    // pushed back so the stack still describes what actually happened.
    //
    // This is the mis-scan case: a staff member checking in a stack of books
    // scans one twice, notices, and backs it out without having to work out
    // what the previous state was.
    Status undoLast(int userId, ActivityEntry* outUndone = nullptr);

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
    BookInventory    inventory;
    UserAccounts     users;
    DueDateHeap      dueDates;
    Recommender      recs;
    ActivityLog      log;        // stack per account
    ReservationQueue holds;      // FIFO queue per title

    int currentDay;
    int nextLoanId;

    // Set while undoLast is applying an inverse operation. The inverse must
    // not push its own activity entry, or undoing a checkout would record a
    // return, and undoing that would re-borrow the book forever.
    bool suppressLogging;

    void record(int userId, ActivityType type, const std::string& isbn);

    // Maps (userId, isbn) to that user's outstanding loan IDs for the title.
    //
    // BUG FIX (Module 6): this was originally a single int, which silently
    // assumed a user could never hold two copies of one title. Nothing in
    // the system enforced that. A second checkout overwrote the first loan
    // ID, orphaning the first heap entry permanently and producing a phantom
    // overdue notice for a book that had already been returned. Storing a
    // list of loan IDs and returning them oldest-first fixes it.
    std::unordered_map<std::string, std::vector<int>> loanIdsFor;
    static std::string loanKey(int userId, const std::string& isbn);
};

#endif // LIBRARY_SYSTEM_H
