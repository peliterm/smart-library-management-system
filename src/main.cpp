// ============================================================================
// main.cpp
// Smart Library Management System - CPSC 315
//
// Test driver for the Module 3/4 core modules. Exercises the hash table, the
// sorted linked list with its index, the integration layer, and the failure
// paths that each is supposed to reject.
// ============================================================================
#include "LibrarySystem.h"
#include <iostream>
#include <iomanip>
#include <string>

static int testsRun = 0;
static int testsPassed = 0;

// Minimal check helper so the output shows which case failed.
static void check(const std::string& label, bool condition) {
    ++testsRun;
    if (condition) {
        ++testsPassed;
        std::cout << "  [PASS] " << label << "\n";
    } else {
        std::cout << "  [FAIL] " << label << "\n";
    }
}

static void section(const std::string& title) {
    std::cout << "\n" << title << "\n"
              << std::string(title.size(), '-') << "\n";
}

// Builds a realistic ISBN-13 with a shared 978-0-262 prefix, which is the
// clustering case the hash function has to handle.
static std::string makeIsbn(int n) {
    std::string tail = std::to_string(100000 + n);
    return "9780262" + tail;
}

int main() {
    LibrarySystem library;

    // -----------------------------------------------------------------------
    section("1. Book inventory: insert and lookup");
    // -----------------------------------------------------------------------
    library.books().insert(Book("9780262134729", "Digital Apollo",
                                "Mindell", "History", 3));
    library.books().insert(Book("9780262529662", "The Apollo Guidance Computer",
                                "O'Brien", "History", 1));
    library.books().insert(Book("9780134685991", "Effective Modern C++",
                                "Meyers", "Computing", 2));
    library.books().insert(Book("9780201633610", "Design Patterns",
                                "Gamma", "Computing", 4));

    check("four records stored", library.books().size() == 4);
    check("lookup by ISBN finds the right title",
          library.books().find("9780262134729")->title == "Digital Apollo");
    check("lookup of an unknown ISBN returns nullptr",
          library.books().find("0000000000000") == nullptr);

    // Re-inserting the same ISBN updates rather than duplicating.
    library.books().insert(Book("9780134685991", "Effective Modern C++",
                                "Meyers", "Computing", 5));
    check("re-insert updates instead of duplicating",
          library.books().size() == 4 &&
          library.books().find("9780134685991")->copiesTotal == 5);

    // -----------------------------------------------------------------------
    section("2. Hash distribution under shared ISBN prefixes");
    // -----------------------------------------------------------------------
    BookInventory stress;
    const int N = 5000;
    for (int i = 0; i < N; ++i) {
        stress.insert(Book(makeIsbn(i), "Title " + std::to_string(i),
                           "Author", "Test", 1));
    }
    std::cout << "  records: " << N
              << ", buckets: " << stress.bucketCount()
              << ", load factor: " << std::fixed << std::setprecision(2)
              << stress.loadFactor()
              << ", longest chain: " << stress.longestChain() << "\n";

    check("table resized to hold the load", stress.loadFactor() <= 0.75);
    check("no chain grew beyond 6 nodes", stress.longestChain() <= 6);
    check("all records retrievable after resizing",
          stress.find(makeIsbn(0)) != nullptr &&
          stress.find(makeIsbn(N / 2)) != nullptr &&
          stress.find(makeIsbn(N - 1)) != nullptr);

    // -----------------------------------------------------------------------
    section("3. User accounts: sorted list and index");
    // -----------------------------------------------------------------------
    library.accounts().addUser(User(1001, "Mike",  "mike@example.edu",  "hash1", Role::STUDENT));
    library.accounts().addUser(User(1003, "Priya", "priya@example.edu", "hash2", Role::STUDENT));
    library.accounts().addUser(User(1004, "Alex",  "alex@example.edu",  "hash3", Role::LIBRARIAN));

    // Out-of-order ID exercises the interior insert path.
    library.accounts().addUser(User(1002, "Sam", "sam@example.edu", "hash4", Role::STUDENT));

    check("four accounts stored", library.accounts().size() == 4);
    check("duplicate ID rejected",
          !library.accounts().addUser(User(1002, "Dup", "d@x.edu", "h", Role::STUDENT)));

    std::vector<User> listed = library.accounts().listAll();
    bool sorted = true;
    for (size_t i = 1; i < listed.size(); ++i) {
        if (listed[i - 1].userId >= listed[i].userId) sorted = false;
    }
    check("list is in ascending ID order after interior insert", sorted);
    check("index and list agree", library.accounts().validate());
    check("lookup by ID returns the right account",
          library.accounts().findUser(1003)->name == "Priya");

    library.accounts().removeUser(1003);
    check("removal drops the account", library.accounts().size() == 3);
    check("removed ID no longer found",
          library.accounts().findUser(1003) == nullptr);
    check("index and list still agree after removal",
          library.accounts().validate());

    // -----------------------------------------------------------------------
    section("4. Integration: checkout and return");
    // -----------------------------------------------------------------------
    Status s = library.checkout(1001, "9780262134729");
    check("valid checkout succeeds", s == Status::OK);
    check("available count decremented",
          library.books().find("9780262134729")->copiesAvailable == 2);
    check("loan recorded on the account",
          library.accounts().findUser(1001)->activeLoanCount() == 1);

    check("checkout with unknown user rejected",
          library.checkout(9999, "9780262134729") == Status::USER_NOT_FOUND);
    check("checkout with unknown ISBN rejected",
          library.checkout(1001, "0000000000000") == Status::BOOK_NOT_FOUND);

    // Exhaust the single copy of the Apollo Guidance Computer.
    library.checkout(1002, "9780262529662");
    check("checkout with no copies left rejected",
          library.checkout(1001, "9780262529662") == Status::NO_COPIES_AVAILABLE);

    // Loan limit: user 1001 already holds 1, add 4 more to reach 5.
    library.checkout(1001, "9780134685991");
    library.checkout(1001, "9780201633610");
    library.books().insert(Book("9781111111111", "Filler A", "Author", "Test", 2));
    library.books().insert(Book("9782222222222", "Filler B", "Author", "Test", 2));
    library.checkout(1001, "9781111111111");
    library.checkout(1001, "9782222222222");
    check("user is at the five-loan limit",
          library.accounts().findUser(1001)->activeLoanCount() == 5);

    library.books().insert(Book("9783333333333", "Filler C", "Author", "Test", 2));
    check("sixth checkout rejected at the limit",
          library.checkout(1001, "9783333333333") == Status::LOAN_LIMIT_REACHED);

    // Return path.
    check("return succeeds",
          library.returnBook(1001, "9780262134729") == Status::OK);
    check("available count restored",
          library.books().find("9780262134729")->copiesAvailable == 3);
    check("returning a book not on the account rejected",
          library.returnBook(1001, "9780262134729") == Status::NOT_CHECKED_OUT);

    // -----------------------------------------------------------------------
    section("5. Sorted views");
    // -----------------------------------------------------------------------
    std::vector<std::string> byTitle = library.isbnsSortedByTitle();
    std::cout << "  By title:\n";
    for (const std::string& isbn : byTitle) {
        Book* b = library.books().find(isbn);
        std::cout << "    " << std::left << std::setw(32) << b->title
                  << b->author << "\n";
    }

    bool titleOrdered = true;
    for (size_t i = 1; i < byTitle.size(); ++i) {
        if (library.books().find(byTitle[i - 1])->title >
            library.books().find(byTitle[i])->title) titleOrdered = false;
    }
    check("title view is ordered", titleOrdered);

    std::vector<std::string> byAuthor = library.isbnsSortedByAuthor();
    bool authorOrdered = true;
    for (size_t i = 1; i < byAuthor.size(); ++i) {
        if (library.books().find(byAuthor[i - 1])->author >
            library.books().find(byAuthor[i])->author) authorOrdered = false;
    }
    check("author view is ordered", authorOrdered);

    // Stability check: the three "Author" filler books keep title order
    // inside the author group because stable_sort preserved the title sort.
    bool stable = true;
    std::string lastTitle;
    for (const std::string& isbn : byAuthor) {
        Book* b = library.books().find(isbn);
        if (b->author == "Author") {
            if (!lastTitle.empty() && lastTitle > b->title) stable = false;
            lastTitle = b->title;
        }
    }
    check("titles stay ordered within an author group (stability)", stable);

    // -----------------------------------------------------------------------
    std::cout << "\n========================================\n"
              << "  " << testsPassed << " of " << testsRun << " checks passed\n"
              << "========================================\n";

    return (testsPassed == testsRun) ? 0 : 1;
}
