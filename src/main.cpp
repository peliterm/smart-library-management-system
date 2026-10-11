// main.cpp - test driver. Unit tests per structure, integration over full
// checkout and return cycles, stress tests, published cryptographic vectors,
// and a regression suite for every defect found during the project.

#include "LibrarySystem.h"
#include "DemoFixture.h"
#include "PasswordHash.h"
#include "Huffman.h"
#include "Sha256.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <map>

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

        // Advance past the first due date only. Reports go through the
        // authorized entry point, so a staff account is needed.
        lib.accounts().addUser(User(90, "Desk", "d@x.edu", "", Role::LIBRARIAN));
        lib.setToday(122);
        std::vector<OverdueNotice> notices;
        check("staff may run the overdue report",
              lib.overdueReport(90, notices) == Status::OK);
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
        lib.overdueReport(90, notices);
        check("both loans overdue, earliest first",
              notices.size() == 2 &&
              notices[0].daysLate > notices[1].daysLate);

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
        lib.accounts().addUser(User(90, "Desk", "d@x.edu", "", Role::LIBRARIAN));
        std::vector<OverdueNotice> afterReturns;
        lib.overdueReport(90, afterReturns);
        check("no phantom overdue notice after both returns",
              afterReturns.empty());
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

    // =======================================================================
    section("15. Regression: defects found in the Module 9 review");
    // =======================================================================
    // An outside review of the Module 9 package found four problems that the
    // 163-check suite did not cover. Each is locked in below.

    // DEFECT 1 - undoing a return re-dated the loan.
    // The inverse of a return called checkout(), which assigns a fresh due
    // date from today. Returning on day 10 and undoing it moved a day-21
    // deadline to day 31, so an undo quietly granted a longer loan.
    {
        LibrarySystem lib;
        lib.books().insert(Book("5560000000001", "Re-dated", "A", "T", 1));
        lib.accounts().addUser(User(1, "P", "p@x.edu", "h", Role::STUDENT));

        lib.setToday(0);
        int loanId = 0;
        lib.checkout(1, "5560000000001", &loanId);
        check("loan starts due on day 21",
              lib.deadlines().peekMin()->dueDay == 21);

        lib.setToday(10);
        lib.returnBook(1, "5560000000001");
        check("undo of the return succeeds", lib.undoLast(1) == Status::OK);
        check("restored loan keeps its original due day",
              lib.deadlines().peekMin() != nullptr &&
              lib.deadlines().peekMin()->dueDay == 21);
        check("restored loan keeps its original loan ID",
              lib.deadlines().peekMin()->loanId == loanId);
        check("invariants hold after restoring a loan", lib.validateAll());
    }

    // DEFECT 2 - undoing the newer of two loans on one title cleared the
    // older one. The activity entry named only the ISBN, and the return path
    // always removed the oldest loan ID for that account and title.
    {
        LibrarySystem lib;
        lib.books().insert(Book("5560000000002", "Two Copies", "A", "T", 2));
        lib.accounts().addUser(User(1, "P", "p@x.edu", "h", Role::STUDENT));

        lib.setToday(0);
        int first = 0;  lib.checkout(1, "5560000000002", &first);
        lib.setToday(5);
        int second = 0; lib.checkout(1, "5560000000002", &second);
        check("two distinct loans on one title", first != second);

        check("undo of the second checkout succeeds",
              lib.undoLast(1) == Status::OK);
        check("exactly one loan remains", lib.deadlines().size() == 1);
        check("the loan that remains is the FIRST one",
              lib.deadlines().peekMin()->loanId == first);
        check("the remaining loan kept its own due day",
              lib.deadlines().peekMin()->dueDay == 21);
        check("invariants hold after undoing a duplicate-title checkout",
              lib.validateAll());
    }

    // DEFECT 3 - a reservation could vanish without being filled. The front
    // patron was dequeued before their checkout was attempted, so a patron at
    // their loan limit lost their place, nobody behind them was offered the
    // copy, and the copy sat on the shelf with a queue still waiting.
    {
        LibrarySystem lib;
        lib.books().insert(Book("5560000000003", "Wanted", "A", "T", 1));
        for (int i = 1; i <= 5; ++i) {
            lib.books().insert(Book("556000000900" + std::to_string(i),
                                    "Filler", "A", "T", 1));
        }
        lib.accounts().addUser(User(1, "Holder",  "a@x.edu", "h", Role::STUDENT));
        lib.accounts().addUser(User(2, "Maxed",   "b@x.edu", "h", Role::STUDENT));
        lib.accounts().addUser(User(3, "Waiting", "c@x.edu", "h", Role::STUDENT));

        lib.checkout(1, "5560000000003");
        for (int i = 1; i <= 5; ++i) {
            lib.checkout(2, "556000000900" + std::to_string(i));
        }
        check("second patron is at the loan limit",
              lib.accounts().findUser(2)->activeLoanCount()
                  == LibrarySystem::MAX_ACTIVE_LOANS);

        lib.reserve(2, "5560000000003");
        lib.reserve(3, "5560000000003");
        check("two patrons waiting",
              lib.reservations().lengthOf("5560000000003") == 2);

        int filledFor = 0;
        lib.returnBook(1, "5560000000003", &filledFor);

        check("the copy went to the next patron who could take it",
              filledFor == 3);
        check("that patron now holds it",
              lib.accounts().findUser(3)->activeLoanCount() == 1);
        check("the blocked patron kept their place in line",
              lib.reservations().isWaiting("5560000000003", 2));
        check("the blocked patron is now at the front",
              lib.reservations().frontOf("5560000000003") == 2);
        check("no copy left sitting on the shelf",
              lib.books().find("5560000000003")->copiesAvailable == 0);
        check("invariants hold after a partial fulfilment", lib.validateAll());
    }

    // DEFECT 4 - permission checks lived only in the interface. The Module 7
    // write-up stated that they ran in the logic layer and that hiding a menu
    // option was presentation rather than security. LibrarySystem had no role
    // logic at all, so hiding the option was the entire control.
    {
        LibrarySystem lib;
        lib.books().insert(Book("5560000000004", "Any Book", "A", "T", 1));
        lib.accounts().addUser(User(1, "Student",   "s@x.edu", "h", Role::STUDENT));
        lib.accounts().addUser(User(2, "Librarian", "l@x.edu", "h", Role::LIBRARIAN));
        lib.accounts().addUser(User(3, "Admin",     "a@x.edu", "h", Role::ADMINISTRATOR));

        check("role test recognises a librarian", lib.isStaff(2));
        check("role test recognises an administrator", lib.isStaff(3));
        check("role test rejects a student", !lib.isStaff(1));
        check("role test rejects an unknown account", !lib.isStaff(999));

        std::vector<OverdueNotice> notices;
        std::vector<std::string>   isbns;

        check("a student is refused the overdue report",
              lib.overdueReport(1, notices) == Status::NOT_AUTHORIZED);
        check("a refused report returns no data", notices.empty());
        check("a student is refused the inventory report",
              lib.inventoryReport(1, isbns) == Status::NOT_AUTHORIZED);
        check("a refused inventory report returns no data", isbns.empty());
        check("a student is refused the hold report",
              lib.holdReport(1, isbns) == Status::NOT_AUTHORIZED);

        check("a librarian is allowed the overdue report",
              lib.overdueReport(2, notices) == Status::OK);
        check("an administrator is allowed the inventory report",
              lib.inventoryReport(3, isbns) == Status::OK);
        check("the allowed report actually returns the catalog",
              isbns.size() == 1);
        check("an unknown account is refused before the role check",
              lib.overdueReport(999, notices) == Status::USER_NOT_FOUND);
    }

    // =======================================================================
    section("16. Cryptographic primitives and password hashing");
    // =======================================================================
    // The primitives are checked against published test vectors first. An
    // unverified hash implementation that merely agrees with itself proves
    // nothing, and every higher-level password check below rests on these.
    {
        using namespace crypto;

        // SHA-256, FIPS 180-4 examples.
        check("SHA-256 of the empty string matches the published digest",
              toHex(sha256(std::string(""))) ==
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        check("SHA-256 of \"abc\" matches the published digest",
              toHex(sha256(std::string("abc"))) ==
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        check("SHA-256 of the 56-character example matches",
              toHex(sha256(std::string(
                  "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"))) ==
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
        check("SHA-256 spans multiple blocks correctly",
              toHex(sha256(std::string(1000, 'a'))) ==
              "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");

        // HMAC-SHA256, RFC 4231 test cases 1 and 6.
        check("HMAC-SHA256 matches RFC 4231 case 1",
              toHex(hmacSha256(std::vector<unsigned char>(20, 0x0b),
                               std::vector<unsigned char>{'H','i',' ','T','h','e','r','e'})) ==
              "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
        {
            std::string msg = "Test Using Larger Than Block-Size Key - Hash Key First";
            check("HMAC-SHA256 hashes an over-long key first (RFC 4231 case 6)",
                  toHex(hmacSha256(std::vector<unsigned char>(131, 0xaa),
                                   std::vector<unsigned char>(msg.begin(), msg.end()))) ==
                  "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
        }

        // PBKDF2-HMAC-SHA256, RFC 7914 section 11.
        {
            std::string salt = "salt";
            check("PBKDF2-HMAC-SHA256 matches RFC 7914 (c=1)",
                  toHex(pbkdf2HmacSha256("passwd",
                        std::vector<unsigned char>(salt.begin(), salt.end()), 1, 64)) ==
                  "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
                  "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
        }
        {
            std::string salt = "NaCl";
            check("PBKDF2-HMAC-SHA256 matches RFC 7914 (c=80000)",
                  toHex(pbkdf2HmacSha256("Password",
                        std::vector<unsigned char>(salt.begin(), salt.end()), 80000, 64)) ==
                  "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56"
                  "a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d");
        }
        check("hex encoding round-trips", toHex(fromHex("00ff10ab")) == "00ff10ab");
    }

    // Password records built on those primitives.
    {
        const std::string pw = "correct horse battery staple";
        const int FAST = pwhash::DEMO_ITERATIONS;

        std::string a = pwhash::hashPassword(pw, FAST);
        std::string b = pwhash::hashPassword(pw, FAST);

        check("the record names its algorithm and factor",
              a.rfind("pbkdf2_sha256$", 0) == 0 && pwhash::looksHashed(a));
        check("the plaintext is nowhere in the record",
              a.find(pw) == std::string::npos);
        check("the same password hashes differently each time (salt works)",
              a != b);
        check("the correct password verifies against both records",
              pwhash::verifyPassword(pw, a) && pwhash::verifyPassword(pw, b));
        check("a one-character difference is rejected",
              !pwhash::verifyPassword("correct horse battery stapl", a));
        check("case matters", !pwhash::verifyPassword("Correct horse battery staple", a));
        check("an empty password is rejected against a real record",
              !pwhash::verifyPassword("", a));
        check("an empty password can itself be hashed and verified",
              pwhash::verifyPassword("", pwhash::hashPassword("", FAST)));
        check("a non-record fails closed rather than matching",
              !pwhash::verifyPassword(pw, "hash"));
        check("the old FNV record format is no longer accepted",
              !pwhash::verifyPassword(pw, "fnv1a$10000$aabb$ccdd"));
        check("a truncated record fails closed",
              !pwhash::verifyPassword(pw, "pbkdf2_sha256$10000$abc"));
        check("a tampered digest fails",
              !pwhash::verifyPassword(pw, a.substr(0, a.size() - 1) +
                                      (a.back() == '0' ? '1' : '0')));
        check("an absurd iteration count is refused rather than computed",
              !pwhash::verifyPassword(pw, "pbkdf2_sha256$999999999$aabb$ccdd"));
        check("the stored iteration count is used, not the constant",
              pwhash::deriveDigest(pw, "aabb", 10)
                  != pwhash::deriveDigest(pw, "aabb", 11));
        check("a different salt gives a different digest",
              pwhash::deriveDigest(pw, "aabb", 50)
                  != pwhash::deriveDigest(pw, "ccdd", 50));

        // One round trip at the real work factor, to prove the default path
        // works end to end and not only the reduced one used above.
        std::string strong = pwhash::hashPassword("production password");
        check("a record at the default factor records 600000 iterations",
              strong.find("$600000$") != std::string::npos);
        check("a record at the default factor verifies",
              pwhash::verifyPassword("production password", strong));
        check("and rejects a wrong password at that factor",
              !pwhash::verifyPassword("production passwerd", strong));
    }

    // Wired into the system, which is where it was missing before.
    {
        LibrarySystem lib;
        lib.accounts().addUser(User(1, "P", "p@x.edu", "", Role::STUDENT));
        check("an account with no password cannot be signed into",
              !lib.authenticate(1, ""));
        check("setting a password succeeds",
              lib.setPassword(1, "letmein", pwhash::DEMO_ITERATIONS) == Status::OK);
        check("the right password authenticates", lib.authenticate(1, "letmein"));
        check("the wrong password does not", !lib.authenticate(1, "letmein2"));
        check("an unknown account never authenticates",
              !lib.authenticate(999, "letmein"));
        check("the account stores a hash, not the password",
              lib.accounts().findUser(1)->passwordHash.find("letmein")
                  == std::string::npos);

        // Two accounts, one password: the stored records must still differ.
        lib.accounts().addUser(User(2, "Q", "q@x.edu", "", Role::STUDENT));
        lib.setPassword(2, "letmein", pwhash::DEMO_ITERATIONS);
        check("identical passwords store differently across accounts",
              lib.accounts().findUser(1)->passwordHash
                  != lib.accounts().findUser(2)->passwordHash);
        check("but both still verify", lib.authenticate(1, "letmein") &&
                                       lib.authenticate(2, "letmein"));
    }

    // =======================================================================
    section("17. Huffman coding over a catalog export");
    // =======================================================================
    {
        // Round-trip on the real export, which is the case that matters.
        LibrarySystem lib;
        buildDemoLibrary(lib);
        const std::string exported = lib.catalogExport();

        check("the catalog export is non-empty", !exported.empty());
        check("the export has one line per title",
              std::count(exported.begin(), exported.end(), '\n')
                  == static_cast<long>(lib.books().size()));

        huffman::Archive archive = huffman::encode(exported);
        check("encoding produced bits", archive.bitCount > 0);
        check("the export round-trips exactly",
              huffman::decode(archive) == exported);
        check("compressed payload is smaller than the original",
              archive.bytes.size() < exported.size());

        std::cout << "         demo export " << exported.size() << " bytes -> "
                  << archive.bytes.size() << " payload + "
                  << (archive.storedBytes() - archive.bytes.size())
                  << " header = " << archive.storedBytes()
                  << ", ratio " << archive.ratio() << "\n";

        // The header costs five bytes per distinct symbol. On an export this
        // small that is larger than the payload it describes, so the archive
        // is bigger than the input even though the coding worked. This is a
        // real property of Huffman, not a defect: the table is a fixed cost
        // amortized over the data, and six books is not enough data.
        check("the header outweighs the payload at demo scale",
              (archive.storedBytes() - archive.bytes.size()) > archive.bytes.size());

        // At a realistic catalog size the header is amortized and the
        // compression shows its true value. This is the case the
        // specification's "storage optimization" actually describes.
        {
            LibrarySystem big;
            for (int i = 0; i < 5000; ++i) {
                big.books().insert(Book(
                    "978026213" + std::to_string(4000 + i),
                    "Introduction to Computing Volume " + std::to_string(i),
                    "Author " + std::to_string(i % 120),
                    (i % 3 == 0) ? "Computing" : (i % 3 == 1 ? "History" : "Science"),
                    (i % 5) + 1, 1960 + (i % 60)));
            }
            const std::string bigExport = big.catalogExport();
            huffman::Archive bigArchive = huffman::encode(bigExport);

            check("the large export round-trips exactly",
                  huffman::decode(bigArchive) == bigExport);
            check("at catalog scale the archive is smaller than the original",
                  bigArchive.ratio() < 1.0);

            std::cout << "         5,000-title export " << bigExport.size()
                      << " bytes -> " << bigArchive.storedBytes()
                      << " bytes stored, ratio " << bigArchive.ratio() << "\n";
        }

        // The defining property: rarer symbols get longer codes.
        std::map<char, std::string> codes = huffman::codeTable(exported);
        std::map<char, uint32_t> freq;
        for (char c : exported) freq[c] += 1;

        char commonest = exported[0], rarest = exported[0];
        for (const auto& f : freq) {
            if (f.second > freq[commonest]) commonest = f.first;
            if (f.second < freq[rarest])    rarest    = f.first;
        }
        check("the commonest symbol has a code no longer than the rarest",
              codes[commonest].size() <= codes[rarest].size());
        check("no code is empty", codes[commonest].size() > 0);

        // Prefix-free is what lets the stream omit separators. Verify it
        // directly rather than trusting the construction.
        bool prefixFree = true;
        for (const auto& a : codes) {
            for (const auto& b : codes) {
                if (a.first == b.first) continue;
                if (b.second.rfind(a.second, 0) == 0) prefixFree = false;
            }
        }
        check("the code set is prefix-free", prefixFree);

        // Edge cases.
        check("empty input round-trips to empty",
              huffman::decode(huffman::encode("")).empty());
        check("a single character round-trips",
              huffman::decode(huffman::encode("x")) == "x");
        check("a run of one repeated symbol round-trips",
              huffman::decode(huffman::encode(std::string(1000, 'a')))
                  == std::string(1000, 'a'));
        check("one repeated symbol compresses heavily",
              huffman::encode(std::string(1000, 'a')).bytes.size() < 200);
        check("text with every byte value round-trips", [] {
            std::string all;
            for (int i = 1; i < 256; ++i) all += static_cast<char>(i);
            return huffman::decode(huffman::encode(all)) == all;
        }());
        check("encoding is deterministic across runs",
              huffman::encode(exported).bytes == huffman::encode(exported).bytes);
    }

    // =======================================================================
    section("18. Sorting by publication date");
    // =======================================================================
    {
        LibrarySystem lib;
        buildDemoLibrary(lib);
        std::vector<std::string> byYear = lib.isbnsSortedByYear();

        check("every title appears in the year view",
              byYear.size() == lib.books().size());

        bool descending = true;
        for (size_t i = 1; i < byYear.size(); ++i) {
            int prev = lib.books().find(byYear[i - 1])->year;
            int cur  = lib.books().find(byYear[i])->year;
            if (prev != 0 && cur != 0 && prev < cur) descending = false;
        }
        check("dated titles run newest first", descending);
        check("the newest title leads",
              lib.books().find(byYear.front())->year == 2014);
        check("the oldest dated title trails",
              lib.books().find(byYear.back())->year == 1988);

        // An undated record must sort last, not first.
        lib.books().insert(Book("5570000000001", "No Date", "Z", "T", 1));
        byYear = lib.isbnsSortedByYear();
        check("an undated title sorts to the end",
              byYear.back() == "5570000000001");
    }

    // =======================================================================
    section("19. Secondary word index on titles and authors");
    // =======================================================================
    {
        LibrarySystem lib;
        buildDemoLibrary(lib);
        BookInventory& inv = lib.books();

        check("the index holds words", inv.indexedWordCount() > 0);

        std::vector<std::string> apollo = inv.findByWord("apollo");
        check("a word query finds both Apollo titles", apollo.size() == 2);
        check("the word query is case-insensitive",
              inv.findByWord("APOLLO").size() == 2);
        check("a word absent from the catalog finds nothing",
              inv.findByWord("zebra").empty());

        // Author words are indexed alongside title words.
        check("an author surname is indexed",
              inv.findByWord("hawking").size() == 1);

        // Punctuation must not decide whether a title is findable.
        check("a name with an apostrophe is tokenized",
              inv.findByWord("brien").size() == 1);

        // Multi-word queries intersect posting lists.
        check("two words intersect to the title holding both",
              inv.search("digital apollo").size() == 1);
        check("two words with no title in common find nothing by index, and "
              "the substring fallback also finds nothing",
              inv.search("hawking apollo").empty());

        // The fallback covers what a token index structurally cannot.
        check("a partial word misses the index but the fallback finds it",
              inv.findByWord("apol").empty() && inv.search("apol").size() == 2);

        // The index has to stay in step with the table through writes.
        inv.insert(Book("5580000000001", "Zebra Patterns", "Quill", "T", 1));
        check("a new record is indexed immediately",
              inv.findByWord("zebra").size() == 1);
        inv.remove("5580000000001");
        check("a removed record leaves no index entry",
              inv.findByWord("zebra").empty());

        // Overwriting a record must retire the old title's words.
        inv.insert(Book("5580000000002", "Old Name", "A", "T", 1));
        check("the first title is indexed", inv.findByWord("old").size() == 1);
        inv.insert(Book("5580000000002", "New Name", "A", "T", 1));
        check("the replaced title's words are gone",
              inv.findByWord("old").empty());
        check("the replacement title's words are present",
              inv.findByWord("new").size() == 1);

        // At scale the index is what makes a title query cheap.
        LibrarySystem big;
        for (int i = 0; i < 5000; ++i) {
            big.books().insert(Book("978026213" + std::to_string(4000 + i),
                                    "Volume " + std::to_string(i),
                                    "Author " + std::to_string(i % 120),
                                    "Computing", 1, 2000));
        }
        check("a word shared by every title returns every title",
              big.books().findByWord("volume").size() == 5000);
        check("a single rare title is found without scanning the catalog",
              big.books().findByWord("4999").size() == 1);
    }

    // =======================================================================
    section("20. Staff management of the catalog and accounts");
    // =======================================================================
    {
        LibrarySystem lib;
        lib.accounts().addUser(User(1, "Student",   "s@x.edu", "", Role::STUDENT));
        lib.accounts().addUser(User(2, "Librarian", "l@x.edu", "", Role::LIBRARIAN));
        lib.books().insert(Book("5590000000001", "Existing", "A", "T", 1, 2000));

        Book fresh("5590000000002", "Brand New", "B", "T", 2, 2021);

        check("a student cannot add a book",
              lib.addBook(1, fresh) == Status::NOT_AUTHORIZED);
        check("the refused add changed nothing",
              lib.books().find("5590000000002") == nullptr);
        check("a librarian can add a book",
              lib.addBook(2, fresh) == Status::OK);
        check("the added book is in the catalog",
              lib.books().find("5590000000002") != nullptr);
        check("the added book is searchable immediately",
              lib.books().findByWord("brand").size() == 1);
        check("adding the same ISBN twice is refused",
              lib.addBook(2, fresh) == Status::ALREADY_EXISTS);

        check("a student cannot remove a book",
              lib.removeBook(1, "5590000000002") == Status::NOT_AUTHORIZED);
        check("removing an unknown ISBN says so",
              lib.removeBook(2, "0000000000000") == Status::BOOK_NOT_FOUND);
        check("a librarian can remove an unused book",
              lib.removeBook(2, "5590000000002") == Status::OK);

        // A book with copies out must not be removable.
        lib.checkout(1, "5590000000001");
        check("a book with copies out cannot be removed",
              lib.removeBook(2, "5590000000001") == Status::STILL_IN_USE);
        check("the refused removal left the record in place",
              lib.books().find("5590000000001") != nullptr);

        // Nor one with a hold queue.
        lib.accounts().addUser(User(3, "Waiter", "w@x.edu", "", Role::STUDENT));
        lib.reserve(3, "5590000000001");
        lib.returnBook(1, "5590000000001");          // fulfils patron 3
        check("the hold was filled by the return",
              lib.accounts().findUser(3)->activeLoanCount() == 1);
        check("still unremovable while that copy is out",
              lib.removeBook(2, "5590000000001") == Status::STILL_IN_USE);
        lib.returnBook(3, "5590000000001");
        check("removable once every copy is back",
              lib.removeBook(2, "5590000000001") == Status::OK);

        // Accounts.
        check("a student cannot create an account",
              lib.addAccount(1, User(9, "New", "n@x.edu", "", Role::STUDENT),
                             "pw") == Status::NOT_AUTHORIZED);
        check("a librarian can create an account",
              lib.addAccount(2, User(9, "New", "n@x.edu", "", Role::STUDENT),
                             "pw") == Status::OK);
        check("the new account can sign in with its password",
              lib.authenticate(9, "pw"));
        check("the new account did not store its password in the clear",
              lib.accounts().findUser(9)->passwordHash.find("pw")
                  == std::string::npos);
        check("a duplicate account ID is refused",
              lib.addAccount(2, User(9, "Dup", "d@x.edu", "", Role::STUDENT),
                             "pw") == Status::ALREADY_EXISTS);

        lib.books().insert(Book("5590000000003", "Held", "C", "T", 1, 2001));
        lib.checkout(9, "5590000000003");
        check("an account holding a loan cannot be removed",
              lib.removeAccount(2, 9) == Status::STILL_IN_USE);
        lib.returnBook(9, "5590000000003");
        check("the account can be removed once it holds nothing",
              lib.removeAccount(2, 9) == Status::OK);
        check("the removed account is gone",
              lib.accounts().findUser(9) == nullptr);
        check("the removed account can no longer authenticate",
              !lib.authenticate(9, "pw"));
        check("invariants hold after management operations", lib.validateAll());
    }

    // =======================================================================
    section("21. Regression: second review round");
    // =======================================================================

    // Two copies of one title must produce two loans, listed once each, with
    // their own deadlines. loansFor walked borrowedIsbns, which holds the
    // ISBN once per copy, and expanded the whole loan list on each pass, so
    // two loans came back as four. The account screen then showed the earlier
    // deadline against both rows.
    {
        LibrarySystem lib;
        lib.books().insert(Book("5600000000001", "Two Copies", "A", "T", 2, 2001));
        lib.accounts().addUser(User(1, "P", "p@x.edu", "", Role::STUDENT));

        lib.setToday(0);
        int first = 0;  lib.checkout(1, "5600000000001", &first);
        lib.setToday(5);
        int second = 0; lib.checkout(1, "5600000000001", &second);

        std::vector<LoanEntry> loans = lib.loansFor(1);
        check("two copies produce exactly two loan entries", loans.size() == 2);
        check("the two entries have distinct loan IDs",
              loans.size() == 2 && loans[0].loanId != loans[1].loanId);

        bool has21 = false, has26 = false;
        for (const LoanEntry& l : loans) {
            if (l.dueDay == 21) has21 = true;
            if (l.dueDay == 26) has26 = true;
        }
        check("each loan carries its own due date", has21 && has26);

        // Undo must still remove the newer loan and leave the older intact.
        check("undo of the second checkout succeeds", lib.undoLast(1) == Status::OK);
        loans = lib.loansFor(1);
        check("one loan remains after the undo", loans.size() == 1);
        check("the remaining loan is the first, still due on day 21",
              loans.size() == 1 && loans[0].loanId == first && loans[0].dueDay == 21);
        check("invariants hold", lib.validateAll());
        (void)second;
    }

    // A patron holding three copies must still be listed once per loan.
    {
        LibrarySystem lib;
        lib.books().insert(Book("5600000000002", "Three Copies", "A", "T", 3, 2002));
        lib.accounts().addUser(User(1, "P", "p@x.edu", "", Role::STUDENT));
        for (int day = 0; day < 3; ++day) {
            lib.setToday(day * 10);
            lib.checkout(1, "5600000000002");
        }
        check("three copies produce three loan entries",
              lib.loansFor(1).size() == 3);
    }

    // The overdue builder is no longer reachable without authorization. The
    // guarded report is the only entry point, and a refusal clears whatever
    // the caller passed in rather than leaving stale rows visible.
    {
        LibrarySystem lib;
        lib.books().insert(Book("5600000000003", "Late", "A", "T", 1, 2003));
        lib.accounts().addUser(User(1, "Student", "s@x.edu", "", Role::STUDENT));
        lib.accounts().addUser(User(2, "Desk",    "d@x.edu", "", Role::LIBRARIAN));

        lib.setToday(0);
        lib.checkout(1, "5600000000003");
        lib.setToday(100);

        std::vector<OverdueNotice> notices;
        check("staff get the overdue list",
              lib.overdueReport(2, notices) == Status::OK && notices.size() == 1);

        // Pre-fill, then be refused: the refusal must not leave data behind.
        check("a student is refused",
              lib.overdueReport(1, notices) == Status::NOT_AUTHORIZED);
        check("the refusal cleared the caller's buffer", notices.empty());
    }

    // -----------------------------------------------------------------------
    std::cout << "\n========================================\n"
              << "  " << testsPassed << " of " << testsRun << " checks passed\n"
              << "========================================\n";

    return (testsPassed == testsRun) ? 0 : 1;
}
