// ============================================================================
// LibrarySystem.cpp
// Smart Library Management System - CPSC 315
// ============================================================================
#include "LibrarySystem.h"
#include <algorithm>

LibrarySystem::LibrarySystem()
    : currentDay(0), nextLoanId(1), suppressLogging(false) {}

std::string statusMessage(Status s) {
    switch (s) {
        case Status::OK:                  return "Operation completed.";
        case Status::USER_NOT_FOUND:      return "No account found with that ID.";
        case Status::BOOK_NOT_FOUND:      return "No book found with that ISBN.";
        case Status::NO_COPIES_AVAILABLE: return "All copies are currently checked out.";
        case Status::LOAN_LIMIT_REACHED:  return "Loan limit of 5 books reached.";
        case Status::NOT_CHECKED_OUT:     return "That book is not on this account.";
        case Status::ALREADY_RESERVED:    return "You are already on the hold list for this title.";
        case Status::NOT_RESERVED:        return "You are not on the hold list for this title.";
        case Status::COPY_IS_AVAILABLE:   return "A copy is on the shelf. Borrow it instead of reserving.";
        case Status::NOTHING_TO_UNDO:     return "There is nothing on this account to undo.";
        case Status::UNDO_NOT_POSSIBLE:   return "That action can no longer be reversed.";
    }
    return "Unknown status.";
}

// Records an action on the account's activity stack, unless an undo is
// currently applying an inverse operation.
void LibrarySystem::record(int userId, ActivityType type,
                           const std::string& isbn) {
    if (suppressLogging) return;
    log.push(userId, ActivityEntry(type, isbn, currentDay));
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
    loanIdsFor[loanKey(userId, isbn)].push_back(loanId);      // append, not overwrite

    record(userId, ActivityType::CHECKOUT, isbn);

    // Borrowing a title this patron was waiting on clears their hold. The
    // alternative leaves them in a line for a book they are already holding,
    // and the next return would be handed to someone who does not need it.
    holds.cancel(isbn, userId);

    if (outLoanId != nullptr) *outLoanId = loanId;
    return Status::OK;
}

// ---------------------------------------------------------------------------
// returnBook: the early-return path. Because the loan ID is known, the heap
// removal is O(log n) instead of the O(n) scan a plain heap would require to
// locate an arbitrary element.
// ---------------------------------------------------------------------------
Status LibrarySystem::returnBook(int userId, const std::string& isbn,
                                 int* outFilledFor) {
    if (outFilledFor != nullptr) *outFilledFor = 0;
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

    // Return the oldest outstanding copy first, so the loan that came due
    // earliest is the one cleared.
    auto loanIt = loanIdsFor.find(loanKey(userId, isbn));
    if (loanIt != loanIdsFor.end() && !loanIt->second.empty()) {
        dueDates.removeLoan(loanIt->second.front());   // O(log n) via index map
        loanIt->second.erase(loanIt->second.begin());
        if (loanIt->second.empty()) {
            loanIdsFor.erase(loanIt);                  // no stale empty entries
        }
    }

    record(userId, ActivityType::RETURN, isbn);

    // --- reservation fulfilment ---
    //
    // If anyone is waiting on this title, the copy just returned goes to the
    // front of the line instead of back on the shelf. This is the whole
    // reason the hold list is a queue: the structure decides who is next, so
    // no code here has to.
    //
    // Limitation worth naming: if the checkout for that patron fails, most
    // likely because they are already at the loan limit, their reservation is
    // dropped and the copy stays on the shelf. A production system would keep
    // the hold and put the copy on a hold shelf with an expiry. Doing that
    // properly needs a notification mechanism and a third copy state, neither
    // of which exists here.
    int nextInLine = 0;
    if (holds.dequeue(isbn, &nextInLine)) {
        bool wasSuppressed = suppressLogging;
        suppressLogging = false;         // the fulfilled loan is real activity
        Status fulfil = checkout(nextInLine, isbn);
        suppressLogging = wasSuppressed;

        if (fulfil == Status::OK && outFilledFor != nullptr) {
            *outFilledFor = nextInLine;
        }
    }

    return Status::OK;
}

// ---------------------------------------------------------------------------
// reserve: join the hold line for a title.
//
// Refused when a copy is on the shelf. A hold list that accepts requests for
// available books fills with entries that are already satisfiable, and the
// first return would be handed to someone who could have walked in and taken
// a copy.
// ---------------------------------------------------------------------------
Status LibrarySystem::reserve(int userId, const std::string& isbn) {
    User* user = users.findUser(userId);
    if (user == nullptr) return Status::USER_NOT_FOUND;

    Book* book = inventory.find(isbn);
    if (book == nullptr) return Status::BOOK_NOT_FOUND;

    if (book->isAvailable()) return Status::COPY_IS_AVAILABLE;

    if (!holds.enqueue(isbn, userId)) return Status::ALREADY_RESERVED;

    record(userId, ActivityType::RESERVE, isbn);
    return Status::OK;
}

Status LibrarySystem::cancelReservation(int userId, const std::string& isbn) {
    User* user = users.findUser(userId);
    if (user == nullptr) return Status::USER_NOT_FOUND;

    if (!holds.cancel(isbn, userId)) return Status::NOT_RESERVED;

    record(userId, ActivityType::CANCEL_RESERVATION, isbn);
    return Status::OK;
}

// ---------------------------------------------------------------------------
// undoLast: pop the activity stack and apply the inverse.
//
// The stack is what makes this safe. Only the most recent action can be
// reversed, so an undo cannot reach past an action that other operations
// have since depended on.
//
// If the inverse fails, the entry goes back on the stack. The log has to
// keep describing what actually happened, not what someone tried to undo.
// ---------------------------------------------------------------------------
Status LibrarySystem::undoLast(int userId, ActivityEntry* outUndone) {
    const ActivityEntry* top = log.peek(userId);
    if (top == nullptr) return Status::NOTHING_TO_UNDO;

    ActivityEntry entry;
    log.pop(userId, &entry);

    suppressLogging = true;
    Status result = Status::UNDO_NOT_POSSIBLE;
    switch (entry.type) {
        case ActivityType::CHECKOUT:
            result = returnBook(userId, entry.isbn);
            break;
        case ActivityType::RETURN:
            result = checkout(userId, entry.isbn);
            break;
        case ActivityType::RESERVE:
            result = holds.cancel(entry.isbn, userId)
                   ? Status::OK : Status::UNDO_NOT_POSSIBLE;
            break;
        case ActivityType::CANCEL_RESERVATION:
            // Rejoins at the back, not at the original position. A queue has
            // no way to reopen a gap in the middle, and faking one would mean
            // storing the old index, which goes stale the moment anyone else
            // leaves the line. Undoing a cancellation costs the patron their
            // place, and the interface says so before asking.
            result = holds.enqueue(entry.isbn, userId)
                   ? Status::OK : Status::UNDO_NOT_POSSIBLE;
            break;
    }
    suppressLogging = false;

    if (result != Status::OK) {
        log.push(userId, entry);            // the action still happened
        return result;
    }

    if (outUndone != nullptr) *outUndone = entry;
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
        && recs.validateTree()
        && log.validate()
        && holds.validate();
}
