// LibrarySystem.cpp

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
        case Status::NOT_AUTHORIZED:      return "This account does not have permission for that.";
        case Status::ALREADY_EXISTS:      return "A record with that identifier already exists.";
        case Status::STILL_IN_USE:        return "That record is still in use and cannot be removed.";
    }
    return "Unknown status.";
}

// Records an action on the account's activity stack, unless an undo is
// currently applying an inverse operation.
void LibrarySystem::record(int userId, ActivityType type,
                           const std::string& isbn,
                           int loanId, int dueDay) {
    if (suppressLogging) return;
    log.push(userId, ActivityEntry(type, isbn, currentDay, loanId, dueDay));
}

// Puts a returned loan back as it was. Not checkout(): that would issue a new
// ID, re-date the loan from today, and record a second co-borrow edge for one
// borrowing event.
Status LibrarySystem::restoreLoan(int userId, const std::string& isbn,
                                  int loanId, int dueDay) {
    User* user = users.findUser(userId);
    if (user == nullptr) return Status::USER_NOT_FOUND;

    Book* book = inventory.find(isbn);
    if (book == nullptr) return Status::BOOK_NOT_FOUND;
    if (!book->isAvailable()) return Status::NO_COPIES_AVAILABLE;

    book->copiesAvailable -= 1;
    user->borrowedIsbns.push_back(isbn);
    dueDates.push(LoanEntry(loanId, userId, isbn, dueDay));

    // Insert in loan-ID order rather than appending. The list is read
    // oldest-first when a patron returns a copy without naming one, and a
    // restored older loan belongs ahead of a newer one that is still out.
    std::vector<int>& ids = loanIdsFor[loanKey(userId, isbn)];
    ids.insert(std::upper_bound(ids.begin(), ids.end(), loanId), loanId);
    return Status::OK;
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

    record(userId, ActivityType::CHECKOUT, isbn, loanId, dueDay);

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
                                 int* outFilledFor, int targetLoanId) {
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

    // Oldest copy by default, which is what a patron handing a book over
    // means. An undo names a specific loan: with two copies of one title,
    // clearing the oldest would reverse the wrong action.
    int clearedLoanId = 0;
    int clearedDueDay = 0;

    auto loanIt = loanIdsFor.find(loanKey(userId, isbn));
    if (loanIt != loanIdsFor.end() && !loanIt->second.empty()) {
        std::vector<int>& ids = loanIt->second;
        auto target = ids.begin();                  // oldest by default

        if (targetLoanId != 0) {
            auto wanted = std::find(ids.begin(), ids.end(), targetLoanId);
            if (wanted != ids.end()) target = wanted;
        }

        clearedLoanId = *target;
        const LoanEntry* entry = dueDates.find(clearedLoanId);
        clearedDueDay = (entry != nullptr) ? entry->dueDay : 0;

        dueDates.removeLoan(clearedLoanId);         // O(log n) via index map
        ids.erase(target);
        if (ids.empty()) {
            loanIdsFor.erase(loanIt);               // no stale empty entries
        }
    }

    // The due date is recorded so undoing this return can restore the loan
    // on its original terms rather than issuing a fresh 21-day loan.
    record(userId, ActivityType::RETURN, isbn, clearedLoanId, clearedDueDay);

    // Keep blocked patrons queued and offer the copy to the next eligible
    // one. Only a checkout that succeeds removes someone from the line.
    for (int waiting : holds.waitingOn(isbn)) {
        bool wasSuppressed = suppressLogging;
        suppressLogging = false;         // the fulfilled loan is real activity
        Status fulfil = checkout(waiting, isbn);      // clears their hold on OK
        suppressLogging = wasSuppressed;

        if (fulfil == Status::OK) {
            if (outFilledFor != nullptr) *outFilledFor = waiting;
            break;                       // the copy is gone; stop offering it
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

// Pops the activity stack and applies the inverse. Only the newest action can
// be reversed. If the inverse fails the entry goes back, because the log must
// describe what happened rather than what someone tried to undo.
Status LibrarySystem::undoLast(int userId, ActivityEntry* outUndone) {
    const ActivityEntry* top = log.peek(userId);
    if (top == nullptr) return Status::NOTHING_TO_UNDO;

    ActivityEntry entry;
    log.pop(userId, &entry);

    suppressLogging = true;
    Status result = Status::UNDO_NOT_POSSIBLE;
    switch (entry.type) {
        case ActivityType::CHECKOUT:
            // Name the loan. Without the ID this clears the oldest loan on
            // the title, which is the wrong one when a patron holds two
            // copies and is undoing the second checkout.
            result = returnBook(userId, entry.isbn, nullptr, entry.loanId);
            break;
        case ActivityType::RETURN:
            // Restore the loan on its original terms. Calling checkout here
            // would issue a new loan ID and reset the due date to today plus
            // the loan period, extending the loan instead of reversing the
            // return.
            result = restoreLoan(userId, entry.isbn, entry.loanId, entry.dueDay);
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
// Permission checks live here, not in the interface, so a second front end
// cannot reach a staff report by simply not drawing the menu line.
bool LibrarySystem::isStaff(int userId) const {
    const UserAccounts& accounts = users;
    const User* u = const_cast<UserAccounts&>(accounts).findUser(userId);
    if (u == nullptr) return false;
    return u->role == Role::LIBRARIAN || u->role == Role::ADMINISTRATOR;
}

bool LibrarySystem::authenticate(int userId,
                                 const std::string& password) const {
    const User* u = const_cast<UserAccounts&>(users).findUser(userId);
    if (u == nullptr) return false;
    return pwhash::verifyPassword(password, u->passwordHash);
}

Status LibrarySystem::setPassword(int userId, const std::string& newPassword,
                                  int iterations) {
    User* u = users.findUser(userId);
    if (u == nullptr) return Status::USER_NOT_FOUND;
    u->passwordHash = pwhash::hashPassword(newPassword, iterations);
    return Status::OK;
}

// Staff only. Every removal checks the record is unreferenced first.
Status LibrarySystem::addBook(int requestedBy, const Book& book) {
    if (users.findUser(requestedBy) == nullptr) return Status::USER_NOT_FOUND;
    if (!isStaff(requestedBy))                  return Status::NOT_AUTHORIZED;
    if (inventory.find(book.isbn) != nullptr)   return Status::ALREADY_EXISTS;

    inventory.insert(book);
    return Status::OK;
}

Status LibrarySystem::removeBook(int requestedBy, const std::string& isbn) {
    if (users.findUser(requestedBy) == nullptr) return Status::USER_NOT_FOUND;
    if (!isStaff(requestedBy))                  return Status::NOT_AUTHORIZED;

    Book* book = inventory.find(isbn);
    if (book == nullptr) return Status::BOOK_NOT_FOUND;

    // Copies out means loans in the heap referencing this ISBN. Removing the
    // record would leave the overdue report printing "(unknown title)" and
    // the patron's account listing a book the catalog does not have.
    if (book->copiesAvailable != book->copiesTotal) return Status::STILL_IN_USE;

    // Anyone waiting on it would wait forever.
    if (holds.lengthOf(isbn) > 0) return Status::STILL_IN_USE;

    inventory.remove(isbn);
    return Status::OK;
}

Status LibrarySystem::addAccount(int requestedBy, const User& user,
                                 const std::string& password) {
    if (users.findUser(requestedBy) == nullptr) return Status::USER_NOT_FOUND;
    if (!isStaff(requestedBy))                  return Status::NOT_AUTHORIZED;
    if (users.findUser(user.userId) != nullptr) return Status::ALREADY_EXISTS;

    users.addUser(user);
    setPassword(user.userId, password);   // never stores the plaintext
    return Status::OK;
}

Status LibrarySystem::removeAccount(int requestedBy, int userId) {
    if (users.findUser(requestedBy) == nullptr) return Status::USER_NOT_FOUND;
    if (!isStaff(requestedBy))                  return Status::NOT_AUTHORIZED;

    User* target = users.findUser(userId);
    if (target == nullptr) return Status::USER_NOT_FOUND;

    // An account with books out cannot be deleted: the loans would stay in
    // the heap with no account to chase.
    if (!target->borrowedIsbns.empty()) return Status::STILL_IN_USE;

    // Nor can one that is still waiting on a title, or the next return would
    // be offered to an account that no longer exists.
    for (const std::string& isbn : holds.titlesWithHolds()) {
        if (holds.isWaiting(isbn, userId)) return Status::STILL_IN_USE;
    }

    // The activity stack belongs to the account and goes with it.
    while (log.pop(userId, nullptr)) {}

    users.removeUser(userId);
    return Status::OK;
}

Status LibrarySystem::overdueReport(int requestedBy,
                                    std::vector<OverdueNotice>& out) {
    out.clear();
    if (users.findUser(requestedBy) == nullptr) return Status::USER_NOT_FOUND;
    if (!isStaff(requestedBy))                  return Status::NOT_AUTHORIZED;
    out = overdueNotices();
    return Status::OK;
}

Status LibrarySystem::inventoryReport(int requestedBy,
                                      std::vector<std::string>& outIsbns) {
    outIsbns.clear();
    if (users.findUser(requestedBy) == nullptr) return Status::USER_NOT_FOUND;
    if (!isStaff(requestedBy))                  return Status::NOT_AUTHORIZED;
    outIsbns = isbnsSortedByAuthor();
    return Status::OK;
}

Status LibrarySystem::holdReport(int requestedBy,
                                 std::vector<std::string>& outIsbns) {
    outIsbns.clear();
    if (users.findUser(requestedBy) == nullptr) return Status::USER_NOT_FOUND;
    if (!isStaff(requestedBy))                  return Status::NOT_AUTHORIZED;
    outIsbns = holds.titlesWithHolds();
    return Status::OK;
}

std::vector<LoanEntry> LibrarySystem::loansFor(int userId) const {
    std::vector<LoanEntry> out;
    const User* u = const_cast<UserAccounts&>(users).findUser(userId);
    if (u == nullptr) return out;

    // borrowedIsbns lists an ISBN once per copy held, so a patron with two
    // copies of one title has it twice. The loan list for that pair already
    // holds both loans, so visiting the ISBN twice would return each loan
    // twice. Each ISBN is expanded once.
    std::unordered_set<std::string> seen;
    for (const std::string& isbn : u->borrowedIsbns) {
        if (!seen.insert(isbn).second) continue;

        auto it = loanIdsFor.find(loanKey(userId, isbn));
        if (it == loanIdsFor.end()) continue;
        for (int loanId : it->second) {
            const LoanEntry* entry = dueDates.find(loanId);
            if (entry != nullptr) out.push_back(*entry);   // O(1) via index map
        }
    }
    return out;
}

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
// Newest first. Title sort first, then a stable sort by year, so titles stay
// ordered inside each year. Undated records are pushed to the end.
std::vector<std::string> LibrarySystem::isbnsSortedByYear() {
    std::vector<std::string> isbns = isbnsSortedByTitle();

    std::stable_sort(isbns.begin(), isbns.end(),
        [this](const std::string& a, const std::string& b) {
            Book* ba = inventory.find(a);
            Book* bb = inventory.find(b);
            int ya = (ba != nullptr) ? ba->year : 0;
            int yb = (bb != nullptr) ? bb->year : 0;
            if (ya == 0 && yb == 0) return false;   // both undated, keep order
            if (ya == 0) return false;              // undated sorts last
            if (yb == 0) return true;
            return ya > yb;                         // newest first
        });
    return isbns;
}

// One record per line, sorted by ISBN: a hash table has no order of its own,
// and an export whose line order varies cannot be compared or hashed.
std::string LibrarySystem::catalogExport() {
    std::vector<Book> all = inventory.getAll();
    std::sort(all.begin(), all.end(),
              [](const Book& a, const Book& b) { return a.isbn < b.isbn; });

    std::string out;
    for (const Book& b : all) {
        out += b.isbn;  out += '|';
        out += b.title; out += '|';
        out += b.author; out += '|';
        out += b.genre; out += '|';
        out += std::to_string(b.year); out += '|';
        out += std::to_string(b.copiesAvailable); out += '/';
        out += std::to_string(b.copiesTotal);
        out += '\n';
    }
    return out;
}

bool LibrarySystem::validateAll() const {
    return users.validate()
        && dueDates.validate()
        && recs.validateTree()
        && log.validate()
        && holds.validate();
}
