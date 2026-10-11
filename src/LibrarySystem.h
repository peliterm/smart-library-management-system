// ============================================================================
// LibrarySystem.h
// Smart Library Management System - CPSC 315
//
// Integration layer. The inventory and account modules never touch each
// other's data; every cross-module operation goes through a function here.
//
// Two rules keep the two modules consistent:
//   1. All validation runs before the first write. A checkout confirms the
//      user exists, is under the loan limit, and that a copy is on the shelf
//      before anything is modified.
//   2. Every operation returns a named reason on failure, not just false, so
//      the interface can tell the user which rule stopped them.
// ============================================================================
#ifndef LIBRARY_SYSTEM_H
#define LIBRARY_SYSTEM_H

#include "BookInventory.h"
#include "UserAccounts.h"
#include <string>
#include <vector>

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

class LibrarySystem {
public:
    LibrarySystem();

    static constexpr size_t MAX_ACTIVE_LOANS = 5;

    BookInventory& books()    { return inventory; }
    UserAccounts&  accounts() { return users; }

    // Checks out one copy. Validates fully, then writes to both modules.
    Status checkout(int userId, const std::string& isbn);

    // Returns a copy and clears the ISBN from the user's loan list.
    Status returnBook(int userId, const std::string& isbn);

    // Sorted views. The sort operates on a vector of ISBNs, not on Book
    // objects, so only small keys move during the sort rather than whole
    // records. Sorting is stable, which lets a caller sort by title and then
    // by author to get titles ordered inside each author group.
    std::vector<std::string> isbnsSortedByTitle();
    std::vector<std::string> isbnsSortedByAuthor();

private:
    BookInventory inventory;
    UserAccounts  users;
};

#endif // LIBRARY_SYSTEM_H
