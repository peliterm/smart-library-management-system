// ============================================================================
// main.cpp
// Smart Library Management System - CPSC 315
//
// Test driver for the Module 3/4 core modules. Exercises the hash table, the
// sorted linked list with its index, the integration layer, and the failure
// paths that each is supposed to reject.
// ============================================================================
#include "LibrarySystem.h"
#include "DemoFixture.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdint>

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
        //
        // The multiply is done in uint32_t, not int. Signed overflow is
        // undefined behavior in C++, and this generator overflows on its
        // first step by design, so the original signed version was relying
        // on behavior the standard does not define. UndefinedBehaviorSanitizer
        // flagged it during the Module 9 review. Unsigned overflow is defined
        // to wrap, which is exactly the arithmetic a linear congruential
        // generator wants.
        uint32_t seed = 12345u;
        for (int i = 0; i < LOANS; ++i) {
            seed = (seed * 1103515245u + 12345u) & 0x7FFFFFFFu;
            heap.push(LoanEntry(i, i % 50, "isbn",
                                static_cast<int>(seed % 10000u)));
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

    // =======================================================================
    section("11. Interface-state coverage of the demo fixture");
    // =======================================================================
    // Added in Module 9, in response to instructor feedback on the Module 8
    // post. The Module 8 dry run turned up two empty screens. Nothing was
    // broken: the recommendation and overdue features had no data to show
    // because the seed contained a catalog and accounts but no borrowing
    // history. Every test passed, because no test knew the screens existed.
    //
    // The checks below close that gap. They assert the property the dry run
    // was relied on to catch, which is that the fixture reaches the state
    // each screen is meant to demonstrate.
    {
        LibrarySystem lib;
        buildDemoLibrary(lib);

        check("fixture leaves the system consistent", lib.validateAll());

        // The point of the section: every dynamic screen has content under
        // the demo fixture, checked by walking the enumerated screen list
        // rather than a hand-maintained set.
        std::vector<ScreenState> coverage = screenCoverage(lib);
        check("coverage check inspects every declared screen",
              coverage.size() == allScreens().size());

        for (const ScreenState& st : coverage) {
            check(std::string("populated under demo fixture: ") + screenName(st.screen),
                  st.populated);
        }

        // Spot-check the counts the demo narration actually quotes, so a
        // fixture change that keeps a screen non-empty but alters what the
        // audience is told still fails here.
        for (const ScreenState& st : coverage) {
            if (st.screen == Screen::OverdueReport) {
                check("overdue report shows the two loans the script names",
                      st.itemCount == 2);
            }
            if (st.screen == Screen::Recommendations) {
                check("recommendations are non-empty and bounded by the limit",
                      st.itemCount >= 1 && st.itemCount <= 5);
            }
        }
    }

    // The empty states are real states, not failures, so they get a fixture
    // of their own rather than being covered by accident. This is the exact
    // condition that produced the Module 8 blank screens, kept deliberately.
    {
        LibrarySystem lib;
        buildEmptyHistoryLibrary(lib);

        check("no-history fixture is consistent", lib.validateAll());

        std::vector<ScreenState> coverage = screenCoverage(lib);
        for (const ScreenState& st : coverage) {
            switch (st.screen) {
                case Screen::Recommendations:
                    check("no-history fixture: recommendations render empty",
                          !st.populated);
                    break;
                case Screen::OverdueReport:
                    check("no-history fixture: overdue report renders empty",
                          !st.populated);
                    break;
                case Screen::SearchResults:
                    check("no-history fixture: search still finds the catalog",
                          st.populated);
                    break;
                case Screen::InventoryTotals:
                    check("no-history fixture: inventory totals still populate",
                          st.populated);
                    break;
                default:
                    break;
            }
        }
    }

    // An empty catalog must not crash the screens that read it. The counts
    // all collapse to zero rather than the code faulting on a missing book.
    {
        LibrarySystem lib;
        buildEmptyLibrary(lib);

        check("empty fixture is consistent", lib.validateAll());

        std::vector<ScreenState> coverage = screenCoverage(lib);
        bool anyPopulated = false;
        for (const ScreenState& st : coverage) {
            if (st.populated) anyPopulated = true;
        }
        check("empty fixture leaves every screen empty without faulting",
              !anyPopulated);
    }

    // =======================================================================
    section("12. Activity log: the per-account stack");
    // =======================================================================
    {
        ActivityLog log;
        check("empty log reports nothing", log.peek(7) == nullptr);
        check("empty log has zero depth", log.depth(7) == 0);
        check("empty log validates", log.validate());

        log.push(7, ActivityEntry(ActivityType::CHECKOUT, "A", 10));
        log.push(7, ActivityEntry(ActivityType::RETURN,   "A", 12));
        log.push(7, ActivityEntry(ActivityType::RESERVE,  "B", 13));

        check("depth tracks pushes", log.depth(7) == 3);
        check("peek returns the newest entry",
              log.peek(7) != nullptr && log.peek(7)->isbn == "B");

        // Newest first is the property the undo and the display both rely on.
        std::vector<ActivityEntry> recent = log.recent(7, 2);
        check("recent returns newest first",
              recent.size() == 2 && recent[0].isbn == "B" && recent[1].isbn == "A");
        check("reading the stack does not consume it", log.depth(7) == 3);

        ActivityEntry popped;
        check("pop returns the newest", log.pop(7, &popped) && popped.isbn == "B");
        check("pop reduces depth", log.depth(7) == 2);

        // Separate accounts must not share a chain.
        log.push(9, ActivityEntry(ActivityType::CHECKOUT, "Z", 14));
        check("accounts keep separate stacks",
              log.depth(7) == 2 && log.depth(9) == 1);
        check("total counts every account", log.totalEntries() == 3);
        check("log validates after mixed operations", log.validate());

        while (log.pop(7, nullptr)) {}
        check("drained account reports empty", log.depth(7) == 0);
        check("drained account has no peek", log.peek(7) == nullptr);
        check("log validates after draining", log.validate());
    }

    // =======================================================================
    section("13. Reservation queue: FIFO hold lines per title");
    // =======================================================================
    {
        ReservationQueue q;
        check("empty queue validates", q.validate());
        check("empty line has no front", q.frontOf("X") == 0);

        check("first patron joins",  q.enqueue("X", 100));
        check("second patron joins", q.enqueue("X", 200));
        check("third patron joins",  q.enqueue("X", 300));
        check("line length tracked", q.lengthOf("X") == 3);
        check("front is the first to ask", q.frontOf("X") == 100);

        // The ordering guarantee is the reason this is a queue.
        std::vector<int> line = q.waitingOn("X");
        check("line is in arrival order",
              line.size() == 3 && line[0] == 100 && line[1] == 200 && line[2] == 300);

        check("a patron cannot take two places", !q.enqueue("X", 200));
        check("refused duplicate left the line unchanged", q.lengthOf("X") == 3);

        // Cancelling from the middle. The tail pointer is the thing that
        // breaks if this is done carelessly, so the next enqueue after a
        // middle removal is the real test.
        check("middle patron cancels", q.cancel("X", 200));
        check("line shortens", q.lengthOf("X") == 2);
        line = q.waitingOn("X");
        check("order survives a middle removal",
              line.size() == 2 && line[0] == 100 && line[1] == 300);

        // Cancelling the tail, then enqueueing again. A stale tail pointer
        // would either lose this entry or write through freed memory.
        check("tail patron cancels", q.cancel("X", 300));
        check("enqueue after tail removal works", q.enqueue("X", 400));
        line = q.waitingOn("X");
        check("new patron lands at the back",
              line.size() == 2 && line[0] == 100 && line[1] == 400);
        check("queue validates after tail churn", q.validate());

        int served = 0;
        check("dequeue serves the front", q.dequeue("X", &served) && served == 100);
        check("dequeue serves in order", q.dequeue("X", &served) && served == 400);
        check("drained line is gone", q.lengthOf("X") == 0);
        check("dequeue on an empty line fails", !q.dequeue("X", &served));
        check("queue validates after draining", q.validate());

        // Separate titles keep separate lines.
        q.enqueue("A", 1);
        q.enqueue("B", 1);
        check("one patron may wait on two titles",
              q.lengthOf("A") == 1 && q.lengthOf("B") == 1);
        check("total counts across titles", q.totalWaiting() == 2);
        check("cancelling one title leaves the other",
              q.cancel("A", 1) && q.lengthOf("B") == 1);
        check("queue validates across titles", q.validate());
    }

    // =======================================================================
    section("14. Integration: reservations, fulfilment, and undo");
    // =======================================================================
    {
        LibrarySystem lib;
        lib.books().insert(Book("5551000000001", "One Copy", "A", "T", 1));
        lib.books().insert(Book("5551000000002", "Two Copies", "B", "T", 2));
        lib.accounts().addUser(User(1, "Holder",  "h@x.edu", "p", Role::STUDENT));
        lib.accounts().addUser(User(2, "Waiter",  "w@x.edu", "p", Role::STUDENT));
        lib.accounts().addUser(User(3, "Waiter2", "v@x.edu", "p", Role::STUDENT));

        // Reserving something on the shelf is refused: borrow it instead.
        check("cannot reserve an available title",
              lib.reserve(2, "5551000000001") == Status::COPY_IS_AVAILABLE);

        lib.checkout(1, "5551000000001");
        check("now the title is reservable",
              lib.reserve(2, "5551000000001") == Status::OK);
        check("reserving twice is refused",
              lib.reserve(2, "5551000000001") == Status::ALREADY_RESERVED);
        check("second patron joins behind the first",
              lib.reserve(3, "5551000000001") == Status::OK);
        check("hold line is two deep",
              lib.reservations().lengthOf("5551000000001") == 2);

        // The point of the queue: the return goes to whoever asked first.
        int filledFor = 0;
        check("return succeeds",
              lib.returnBook(1, "5551000000001", &filledFor) == Status::OK);
        check("returned copy went to the front of the line", filledFor == 2);
        check("fulfilled patron now holds it",
              lib.accounts().findUser(2)->activeLoanCount() == 1);
        check("fulfilled patron left the line",
              !lib.reservations().isWaiting("5551000000001", 2));
        check("the next patron moved up",
              lib.reservations().frontOf("5551000000001") == 3);
        check("no copy sitting on the shelf after fulfilment",
              lib.books().find("5551000000001")->copiesAvailable == 0);
        check("invariants hold after fulfilment", lib.validateAll());

        // Waiting on one title is unaffected by borrowing a different one.
        lib.checkout(3, "5551000000002");
        check("borrowing another title does not forfeit a hold",
              lib.reservations().isWaiting("5551000000001", 3));

        // But borrowing the title you were waiting on does clear the hold,
        // or the next return would be handed to someone already holding a
        // copy while a genuinely waiting patron stays in line behind them.
        {
            LibrarySystem c;
            c.books().insert(Book("5554000000001", "Two Up", "A", "T", 2));
            c.accounts().addUser(User(1, "Had It",  "a@x.edu", "p", Role::STUDENT));
            c.accounts().addUser(User(2, "Wanted It","b@x.edu", "p", Role::STUDENT));

            c.checkout(1, "5554000000001");
            c.checkout(2, "5554000000001");       // both copies out
            c.accounts().addUser(User(3, "Waiting", "c@x.edu", "p", Role::STUDENT));
            check("title with no copies can be reserved",
                  c.reserve(3, "5554000000001") == Status::OK);

            c.returnBook(1, "5554000000001");     // fulfils patron 3
            check("fulfilment cleared the hold",
                  c.reservations().lengthOf("5554000000001") == 0);
            check("fulfilled patron holds the copy",
                  c.accounts().findUser(3)->activeLoanCount() == 1);
            check("invariants hold after fulfilment by return", c.validateAll());
        }

        // Undo: the mis-scan case.
        LibrarySystem u;
        u.books().insert(Book("5552000000001", "Undo Me", "A", "T", 2));
        u.accounts().addUser(User(1, "Staff", "s@x.edu", "p", Role::LIBRARIAN));

        check("nothing to undo on a fresh account",
              u.undoLast(1) == Status::NOTHING_TO_UNDO);

        u.checkout(1, "5552000000001");
        check("checkout recorded on the stack", u.activity().depth(1) == 1);

        ActivityEntry undone;
        check("undo reverses the checkout",
              u.undoLast(1, &undone) == Status::OK);
        check("undo reported the right action",
              undone.type == ActivityType::CHECKOUT);
        check("copy went back on the shelf",
              u.books().find("5552000000001")->copiesAvailable == 2);
        check("loan cleared from the account",
              u.accounts().findUser(1)->activeLoanCount() == 0);
        check("heap entry cleared", u.deadlines().empty());

        // The inverse operation must not log itself, or undo would never
        // terminate and the stack would grow on every reversal.
        check("undo did not record itself", u.activity().depth(1) == 0);
        check("stack is empty, so nothing is left to undo",
              u.undoLast(1) == Status::NOTHING_TO_UNDO);
        check("invariants hold after undo", u.validateAll());

        // A failed inverse must leave the log describing what really
        // happened, rather than quietly dropping the entry.
        LibrarySystem f;
        f.books().insert(Book("5553000000001", "Last Copy", "A", "T", 1));
        f.accounts().addUser(User(1, "First",  "a@x.edu", "p", Role::STUDENT));
        f.accounts().addUser(User(2, "Second", "b@x.edu", "p", Role::STUDENT));

        f.checkout(1, "5553000000001");
        f.returnBook(1, "5553000000001");
        f.checkout(2, "5553000000001");          // someone else took it
        check("the only copy is now held by someone else",
              f.books().find("5553000000001")->copiesAvailable == 0);

        size_t before = f.activity().depth(1);
        check("undoing the return fails, the copy is gone",
              f.undoLast(1) == Status::NO_COPIES_AVAILABLE);
        check("failed undo put the entry back",
              f.activity().depth(1) == before);
        check("invariants hold after a failed undo", f.validateAll());
    }

    // -----------------------------------------------------------------------
    std::cout << "\n========================================\n"
              << "  " << testsPassed << " of " << testsRun << " checks passed\n"
              << "========================================\n";

    return (testsPassed == testsRun) ? 0 : 1;
}
