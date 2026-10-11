// ============================================================================
// LibrarySystem.cpp
// Smart Library Management System - CPSC 315
// ============================================================================
#include "LibrarySystem.h"
#include <algorithm>

LibrarySystem::LibrarySystem()
    : currentDay(0), nextLoanId(1) {}

std::string statusMessage(Status s) {
    switch (s) {
        case Status::OK:                  return "Operation completed.";
        case Status::USER_NOT_FOUND:      return "No account found with that ID.";
        case Status::BOOK_NOT_FOUND:      return "No book found with that ISBN.";
        case Status::NO_COPIES_AVAILABLE: return "All copies are currently checked out.";
        case Status::LOAN_LIMIT_REACHED:  return "Loan limit of 5 books reached.";
        case Status::NOT_CHECKED_OUT:     return "That book is not on this account.";
    }
    return "Unknown status.";
}

std::string LibrarySystem::loanKey(int userId, const std::string& isbn) {
    return std::to_string(userId) + "|" + isbn;
}

// ---------------------------------------------------------------------------
// checkout: every check runs before the first write, so a failure partway
// through cannot leave the modules disagreeing.
//
// Cost: two O(1) average lookups, one O(log n) heap insert, and graph edge
// updates proportional to the user's current loan count. The heap insert
// dominates.
// ---------------------------------------------------------------------------
Status LibrarySystem::checkout(int userId, const std::string& isbn,
                               int* outLoanId) {
    // --- validation phase, no writes ---
    User* user = users.findUser(userId);                 // O(1) average
    if (user == nullptr) {
        return Status::USER_NOT_FOUND;
    }
    if (user->activeLoanCount() >= MAX_ACTIVE_LOANS) {
        return Status::LOAN_LIMIT_REACHED;
    }

    Book* book = inventory.find(isbn);                   // O(1) average
    if (book == nullptr) {
        return Status::BOOK_NOT_FOUND;
    }
    if (!book->isAvailable()) {
        return Status::NO_COPIES_AVAILABLE;
    }

    // --- write phase: inventory, account, deadline heap, graph ---
    int loanId = nextLoanId++;
    int dueDay = currentDay + LOAN_PERIOD_DAYS;

    // Strengthen co-borrow edges against everything already on this account
    // before the new ISBN is appended to the history.
    for (const std::string& previous : user->borrowedIsbns) {
        recs.recordCoBorrow(previous, isbn);
    }

    book->copiesAvailable -= 1;
    user->borrowedIsbns.push_back(isbn);
    dueDates.push(LoanEntry(loanId, userId, isbn, dueDay));   // O(log n)
    loanIdFor[loanKey(userId, isbn)] = loanId;

    if (outLoanId != nullptr) *outLoanId = loanId;
    return Status::OK;
}

// ---------------------------------------------------------------------------
// returnBook: the early-return path. Because the loan ID is known, the heap
// removal is O(log n) instead of the O(n) scan a plain heap would require to
// locate an arbitrary element.
// ---------------------------------------------------------------------------
Status LibrarySystem::returnBook(int userId, const std::string& isbn) {
    User* user = users.findUser(userId);
    if (user == nullptr) {
        return Status::USER_NOT_FOUND;
    }

    auto it = std::find(user->borrowedIsbns.begin(),
                        user->borrowedIsbns.end(), isbn);
    if (it == user->borrowedIsbns.end()) {
        return Status::NOT_CHECKED_OUT;
    }

    Book* book = inventory.find(isbn);
    if (book == nullptr) {
        return Status::BOOK_NOT_FOUND;
    }

    // --- write phase ---
    user->borrowedIsbns.erase(it);
    if (book->copiesAvailable < book->copiesTotal) {
        book->copiesAvailable += 1;
    }

    auto loanIt = loanIdFor.find(loanKey(userId, isbn));
    if (loanIt != loanIdFor.end()) {
        dueDates.removeLoan(loanIt->second);     // O(log n) via index map
        loanIdFor.erase(loanIt);
    }
    return Status::OK;
}

// ---------------------------------------------------------------------------
// overdueNotices: the heap hands back loans in due-date order and stops at
// the first one not yet due, so the cost is proportional to the number of
// overdue items rather than the number of active loans.
// ---------------------------------------------------------------------------
std::vector<OverdueNotice> LibrarySystem::overdueNotices() {
    std::vector<LoanEntry> late = dueDates.overdueAsOf(currentDay);
    std::vector<OverdueNotice> notices;
    notices.reserve(late.size());

    for (const LoanEntry& loan : late) {
        OverdueNotice n;
        n.userId   = loan.userId;
        n.isbn     = loan.isbn;
        n.daysLate = currentDay - loan.dueDay;

        User* u = users.findUser(loan.userId);
        n.userName = (u != nullptr) ? u->name : "(unknown account)";

        Book* b = inventory.find(loan.isbn);
        n.title = (b != nullptr) ? b->title : "(unknown title)";

        notices.push_back(n);
    }
    return notices;
}

std::vector<Recommendation> LibrarySystem::recommendationsFor(int userId,
                                                              size_t limit) {
    User* user = users.findUser(userId);
    if (user == nullptr) {
        return std::vector<Recommendation>();
    }
    return recs.recommend(user->borrowedIsbns, limit);
}

// ---------------------------------------------------------------------------
// Sorted views. std::stable_sort is used rather than std::sort so that a
// previous ordering survives the next sort on a different field.
// ---------------------------------------------------------------------------
std::vector<std::string> LibrarySystem::isbnsSortedByTitle() {
    std::vector<Book> all = inventory.getAll();
    std::vector<std::string> isbns;
    isbns.reserve(all.size());
    for (const Book& b : all) isbns.push_back(b.isbn);

    std::stable_sort(isbns.begin(), isbns.end(),
        [this](const std::string& a, const std::string& b) {
            return inventory.find(a)->title < inventory.find(b)->title;
        });
    return isbns;
}

std::vector<std::string> LibrarySystem::isbnsSortedByAuthor() {
    std::vector<std::string> isbns = isbnsSortedByTitle();   // title order first

    // Stable sort by author preserves the title ordering inside each group.
    std::stable_sort(isbns.begin(), isbns.end(),
        [this](const std::string& a, const std::string& b) {
            return inventory.find(a)->author < inventory.find(b)->author;
        });
    return isbns;
}

// ---------------------------------------------------------------------------
// validateAll: one call that runs each module's own invariant check.
// ---------------------------------------------------------------------------
bool LibrarySystem::validateAll() const {
    return users.validate()
        && dueDates.validate()
        && recs.validateTree();
}
