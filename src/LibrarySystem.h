// LibrarySystem.h - coordination layer. Structure modules never touch each
// other's data; every cross-module operation goes through a function here.
//
// Two rules keep them consistent:
//   1. All validation runs before the first write.
//   2. Operations return a named Status, not a bool, so the interface can say
//      which rule stopped the user.

#ifndef LIBRARY_SYSTEM_H
#define LIBRARY_SYSTEM_H

#include "BookInventory.h"
#include "UserAccounts.h"
#include "DueDateHeap.h"
#include "Recommender.h"
#include "ActivityLog.h"
#include "ReservationQueue.h"
#include "PasswordHash.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

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
    UNDO_NOT_POSSIBLE,     // the action could not be reversed from here
    NOT_AUTHORIZED,        // the account's role does not permit this
    ALREADY_EXISTS,        // an ISBN or account ID already in use
    STILL_IN_USE           // copies are out, or the account still holds loans
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
    //
    // targetLoanId names a specific loan to clear. Callers pass 0, meaning
    // "the oldest copy on this account", which is what a patron handing a
    // book over at the desk means. Undo passes the exact loan it is
    // reversing, because a patron holding two copies of one title has two
    // loans the ISBN alone cannot tell apart.
    Status returnBook(int userId, const std::string& isbn,
                      int* outFilledFor = nullptr,
                      int targetLoanId = 0);

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

    // ---- staff-only reporting ----
    //
    // Added in the Module 9 review. The Module 7 write-up said permission
    // checks run in the logic layer and that hiding a menu option is
    // presentation rather than security. That was the intended design and it
    // was not what the code did: role was consulted only by `LibraryUI` when
    // deciding which menu lines to print, so the hidden options were the
    // whole of the access control.
    //
    // These take the requesting account and refuse with NOT_AUTHORIZED when
    // its role is not LIBRARIAN or ADMINISTRATOR, so the rule holds whatever
    // the interface above chooses to display.
    //
    // Scope, stated plainly: the console passes the authenticated account ID
    // to these methods, which enforce the role check here. The low-level
    // structure accessors above remain public for fixtures and internal use,
    // so the project does not claim that every public method enforces
    // authorization. That is a limitation of a teaching implementation.
    bool isStaff(int userId) const;

    // ---- authentication ----
    //
    // Verifies a password against the account's stored hash record. Returns
    // false for an unknown account and for an account whose stored field is
    // not a hash record, so an account created without a password cannot be
    // signed into rather than being signed into freely.
    bool authenticate(int userId, const std::string& password) const;

    // Sets or replaces an account's password, storing only the salted,
    // iterated digest. The plaintext is never held anywhere.
    Status setPassword(int userId, const std::string& newPassword,
                       int iterations = pwhash::DEFAULT_ITERATIONS);

    // ---- staff-only catalog and account management ----
    //
    // The structures have supported adding and removing records since Module
    // 4, but nothing above them exposed it, so a librarian could not add a
    // book without editing the seed data. These are the guarded operations
    // the interface calls.
    //
    // Both removals refuse while the record is in use. Deleting a book whose
    // copies are out, or an account still holding loans, would leave the
    // due-date heap pointing at a record that no longer exists, which is the
    // same class of dangling reference the Module 6 defects were.
    Status addBook(int requestedBy, const Book& book);
    Status removeBook(int requestedBy, const std::string& isbn);
    Status addAccount(int requestedBy, const User& user,
                      const std::string& password);
    Status removeAccount(int requestedBy, int userId);

    Status overdueReport(int requestedBy, std::vector<OverdueNotice>& out);
    Status inventoryReport(int requestedBy, std::vector<std::string>& outIsbns);
    Status holdReport(int requestedBy, std::vector<std::string>& outIsbns);

    // Every outstanding loan on one account, so the interface can show a
    // patron when each of their own books is due. No permission check is
    // needed: the UI only ever asks for the signed-in account, and a patron
    // seeing their own loans is not a privileged read.
    std::vector<LoanEntry> loansFor(int userId) const;

    // Top recommendations for a user, drawn from the co-borrowing graph and
    // ranked through the score BST.
    std::vector<Recommendation> recommendationsFor(int userId, size_t limit);

    // Sorted views. The sort operates on a vector of ISBNs, not on Book
    // objects, so only small keys move during the sort rather than whole
    // records. Sorting is stable, which lets a caller sort by title and then
    // by author to get titles ordered inside each author group.
    std::vector<std::string> isbnsSortedByTitle();
    std::vector<std::string> isbnsSortedByAuthor();

    // Newest first, which is the order a "what's new" shelf wants. Undated
    // records sort last rather than to the beginning, so a missing year does
    // not masquerade as the oldest book in the catalog. Built on the title
    // sort, so titles stay ordered within a single year.
    std::vector<std::string> isbnsSortedByYear();

    // ---- catalog export, the input Huffman coding compresses ----
    //
    // One record per line, fields separated by '|', ordered by ISBN so the
    // export is reproducible. This is the closest thing the system has to the
    // archived data the specification wants compressed: there is no
    // persistence, so nothing is written to disk, but the export is the exact
    // byte sequence that would be.
    std::string catalogExport();

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

    // Builds the overdue list. Private: overdueReport() authorizes first.
    std::vector<OverdueNotice> overdueNotices();

    void record(int userId, ActivityType type, const std::string& isbn,
                int loanId = 0, int dueDay = 0);

    // Re-creates a loan that an undo is restoring, keeping its original loan
    // ID and due date. A fresh checkout cannot do this: it would allocate a
    // new ID and re-date the loan from today, turning "undo this return"
    // into "grant another 21 days".
    Status restoreLoan(int userId, const std::string& isbn,
                       int loanId, int dueDay);

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
