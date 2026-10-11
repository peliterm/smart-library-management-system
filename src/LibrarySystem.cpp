// ============================================================================
// LibrarySystem.cpp
// Smart Library Management System - CPSC 315
// ============================================================================
#include "LibrarySystem.h"
#include <algorithm>

LibrarySystem::LibrarySystem() {}

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

// ---------------------------------------------------------------------------
// checkout: every check runs before the first write, so a failure partway
// through cannot leave the inventory and the account disagreeing.
// ---------------------------------------------------------------------------
Status LibrarySystem::checkout(int userId, const std::string& isbn) {
    // --- validation phase, no writes ---
    User* user = users.findUser(userId);                 // O(1) via index
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

    // --- write phase, both updates happen together ---
    book->copiesAvailable -= 1;
    user->borrowedIsbns.push_back(isbn);
    return Status::OK;
}

// ---------------------------------------------------------------------------
// returnBook: confirms the loan exists on this account before restoring the
// copy, so a mistyped ISBN cannot inflate the available count.
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
    return Status::OK;
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
