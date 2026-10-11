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
    check("all module invariants hold after the checkout sequence",
          library.validateAll());


    // -----------------------------------------------------------------------
    section("5. Due-date heap: deadlines and early returns");
    // -----------------------------------------------------------------------
    {
        LibrarySystem lib;
        lib.setToday(100);
        lib.books().insert(Book("1000000000001", "Book One",   "A", "T", 2));
        lib.books().insert(Book("1000000000002", "Book Two",   "B", "T", 2));
        lib.books().insert(Book("1000000000003", "Book Three", "C", "T", 2));
        lib.accounts().addUser(User(1, "Reader One", "r1@x.edu", "h", Role::STUDENT));
        lib.accounts().addUser(User(2, "Reader Two", "r2@x.edu", "h", Role::STUDENT));

        int loanA = 0, loanB = 0, loanC = 0;
        lib.checkout(1, "1000000000001", &loanA);   // due day 121
        lib.setToday(105);
        lib.checkout(1, "1000000000002", &loanB);   // due day 126
        lib.setToday(102);
        lib.checkout(2, "1000000000003", &loanC);   // due day 123

        check("three loans tracked in the heap", lib.deadlines().size() == 3);
        check("heap invariant and index map hold", lib.deadlines().validate());
        check("earliest due date surfaces first",
              lib.deadlines().peekMin()->loanId == loanA);

        // Early return of the loan sitting in the middle of the heap. This is
        // the arbitrary removal a plain heap cannot do in O(log n).
        lib.setToday(110);
        check("early return succeeds", lib.returnBook(2, "1000000000003") == Status::OK);
        check("heap shrank by one", lib.deadlines().size() == 2);
        check("heap still valid after arbitrary removal", lib.deadlines().validate());
        check("removed loan no longer findable",
              lib.deadlines().find(loanC) == nullptr);
        check("earliest due date unchanged by the removal",
              lib.deadlines().peekMin()->loanId == loanA);

        // Advance past the first due date only.
        lib.setToday(122);
        std::vector<OverdueNotice> notices = lib.overdueNotices();
        check("one loan reported overdue", notices.size() == 1);
        check("overdue notice names the right account and title",
              notices.size() == 1 && notices[0].userId == 1 &&
              notices[0].title == "Book One");
        check("days late computed correctly",
              notices.size() == 1 && notices[0].daysLate == 1);
        check("generating a report does not drain the heap",
              lib.deadlines().size() == 2);

        // Advance past both.
        lib.setToday(130);
        check("both loans overdue, earliest first",
              lib.overdueNotices().size() == 2 &&
              lib.overdueNotices()[0].daysLate > lib.overdueNotices()[1].daysLate);

        (void)loanB;
    }

    // -----------------------------------------------------------------------
    section("6. Heap stress: 2,000 loans in random due-date order");
    // -----------------------------------------------------------------------
    {
        DueDateHeap heap;
        const int LOANS = 2000;
        // Deterministic pseudo-random spread so the test is repeatable.
        int seed = 12345;
        for (int i = 0; i < LOANS; ++i) {
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
            heap.push(LoanEntry(i, i % 50, "isbn", seed % 10000));
        }
        check("all loans pushed", heap.size() == static_cast<size_t>(LOANS));
        check("heap valid after 2,000 inserts", heap.validate());

        // Remove every third loan by ID, which exercises arbitrary removal
        // across the whole array rather than just the root.
        int removed = 0;
        for (int i = 0; i < LOANS; i += 3) {
            if (heap.removeLoan(i)) ++removed;
        }
        check("arbitrary removals succeeded", removed == (LOANS + 2) / 3);
        check("heap valid after scattered removals", heap.validate());

        // Draining must come out in ascending due-date order.
        LoanEntry prev, cur;
        bool ascending = true;
        if (heap.popMin(prev)) {
            while (heap.popMin(cur)) {
                if (cur.dueDay < prev.dueDay) ascending = false;
                prev = cur;
            }
        }
        check("draining yields ascending due dates", ascending);
        check("heap empty after drain", heap.empty());
    }

    // -----------------------------------------------------------------------
    section("7. Recommender: co-borrowing graph and score BST");
    // -----------------------------------------------------------------------
    {
        LibrarySystem lib;
        lib.setToday(0);
        for (int i = 1; i <= 6; ++i) {
            lib.books().insert(Book("200000000000" + std::to_string(i),
                                    "Title " + std::to_string(i),
                                    "Author", "Test", 10));
        }
        for (int u = 1; u <= 4; ++u) {
            lib.accounts().addUser(User(u, "User " + std::to_string(u),
                                        "u@x.edu", "h", Role::STUDENT));
        }

        // Users 1-3 all borrow books 1 and 2, building a strong edge.
        // Users 1-2 also borrow book 3. User 4 borrows book 1 only.
        for (int u = 1; u <= 3; ++u) {
            lib.checkout(u, "2000000000001");
            lib.checkout(u, "2000000000002");
        }
        lib.checkout(1, "2000000000003");
        lib.checkout(2, "2000000000003");
        lib.checkout(4, "2000000000001");

        check("graph has vertices for the borrowed titles",
              lib.recommender().vertexCount() >= 3);
        check("score BST is a valid BST", lib.recommender().validateTree());

        // User 4 has only book 1. Books 2 and 3 are its neighbors, and
        // book 2 has the heavier edge (3 shared borrowers vs 2).
        std::vector<Recommendation> r4 = lib.recommendationsFor(4, 5);
        check("user with one book gets recommendations", !r4.empty());
        check("highest-scoring recommendation comes first",
              r4.size() >= 2 && r4[0].score >= r4[1].score);
        check("top recommendation is the strongest co-borrow",
              !r4.empty() && r4[0].isbn == "2000000000002");

        bool excludesOwn = true;
        for (const Recommendation& r : r4) {
            if (r.isbn == "2000000000001") excludesOwn = false;
        }
        check("books already borrowed are excluded", excludesOwn);

        std::vector<Recommendation> limited = lib.recommendationsFor(4, 1);
        check("limit is respected", limited.size() == 1);

        check("recommender tree valid after traversal",
              lib.recommender().validateTree());
        check("all module invariants hold after recommendation run",
              lib.validateAll());
    }

    // -----------------------------------------------------------------------
    section("8. Recommender: neighbor cap bounds the graph");
    // -----------------------------------------------------------------------
    {
        Recommender rec;
        // One title co-borrowed with 200 others. The cap should hold at 50.
        for (int i = 0; i < 200; ++i) {
            rec.recordCoBorrow("HUB", "N" + std::to_string(i));
        }
        check("neighbor list capped at 50",
              rec.edgeCountFor("HUB") == Recommender::MAX_NEIGHBORS);
        check("tree still valid", rec.validateTree());
    }

    // -----------------------------------------------------------------------
    section("9. Sorted views");
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
    section("10. Regression: bugs found during Module 6 testing");
    // -----------------------------------------------------------------------
    // BUG 1 - Orphaned heap entry on a duplicate-title loan.
    // Cause: the loan lookup stored one loan ID per (user, ISBN) pair, which
    // assumed a user could never hold two copies of the same title. Nothing
    // enforced that. The second checkout overwrote the first loan ID, so the
    // first heap entry could never be removed and generated a phantom overdue
    // notice for a book that had already come back.
    {
        LibrarySystem lib;
        lib.setToday(0);
        lib.books().insert(Book("5550000000001", "Two Copies", "A", "T", 3));
        lib.accounts().addUser(User(1, "Reader", "r@x.edu", "h", Role::STUDENT));

        int first = 0, second = 0;
        lib.checkout(1, "5550000000001", &first);
        lib.setToday(5);
        lib.checkout(1, "5550000000001", &second);
        check("two copies of one title produce distinct loans", first != second);
        check("both loans tracked in the heap", lib.deadlines().size() == 2);

        lib.returnBook(1, "5550000000001");
        check("first return clears exactly one heap entry",
              lib.deadlines().size() == 1);
        lib.returnBook(1, "5550000000001");
        check("second return empties the heap", lib.deadlines().empty());

        lib.setToday(500);
        check("no phantom overdue notice after both returns",
              lib.overdueNotices().empty());
        check("copy count fully restored",
              lib.books().find("5550000000001")->copiesAvailable == 3);
        check("invariants hold after duplicate-title cycle", lib.validateAll());
    }

    // BUG 2 - Asymmetric edges after neighbor trimming.
    // Cause: trimming removed dropped edges from one endpoint only. Because
    // the co-borrowing graph is undirected, every trimmed neighbor still held
    // a return edge to a vertex that had forgotten it, so the 50-neighbor cap
    // stopped bounding the walk in one direction.
    {
        Recommender rec;
        for (int i = 0; i < 200; ++i) {
            rec.recordCoBorrow("HUB", "N" + std::to_string(i));
        }
        check("hub capped at 50 neighbors",
              rec.edgeCountFor("HUB") == Recommender::MAX_NEIGHBORS);

        size_t stillLinked = 0;
        for (int i = 0; i < 200; ++i) {
            if (rec.edgeCountFor("N" + std::to_string(i)) > 0) ++stillLinked;
        }
        check("trimmed neighbors dropped their return edges",
              stillLinked == Recommender::MAX_NEIGHBORS);
        check("tree valid after trimming", rec.validateTree());
    }

    // BUG 3 (near miss) - a return by the wrong account must not restore a
    // copy. This one already behaved correctly; the test locks it in.
    {
        LibrarySystem lib;
        lib.books().insert(Book("5550000000002", "Single", "A", "T", 1));
        lib.accounts().addUser(User(1, "Owner",     "o@x.edu", "h", Role::STUDENT));
        lib.accounts().addUser(User(2, "Not Owner", "n@x.edu", "h", Role::STUDENT));
        lib.checkout(1, "5550000000002");
        check("wrong account cannot return the book",
              lib.returnBook(2, "5550000000002") == Status::NOT_CHECKED_OUT);
        check("failed return does not inflate the copy count",
              lib.books().find("5550000000002")->copiesAvailable == 0);
    }

    // -----------------------------------------------------------------------
    std::cout << "\n========================================\n"
              << "  " << testsPassed << " of " << testsRun << " checks passed\n"
              << "========================================\n";

    return (testsPassed == testsRun) ? 0 : 1;
}
