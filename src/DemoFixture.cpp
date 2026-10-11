// ============================================================================
// DemoFixture.cpp
// Smart Library Management System - CPSC 315
// ============================================================================
#include "DemoFixture.h"
#include <algorithm>
#include <cctype>

namespace demo {
    const std::string SEARCH_TERM      = "apollo";
    const std::string BORROWED_ISBN    = "9780262134729";  // Digital Apollo
    const std::string UNAVAILABLE_ISBN = "9780262529662";  // Apollo Guidance Computer
}

const char* screenName(Screen s) {
    switch (s) {
        case Screen::SearchResults:    return "search results";
        case Screen::UnavailableTitle: return "unavailable title (refusal path)";
        case Screen::Recommendations:  return "recommendations";
        case Screen::AccountLoans:     return "account loans";
        case Screen::OverdueReport:    return "overdue report";
        case Screen::InventoryTotals:  return "inventory totals";
        case Screen::RecentActivity:   return "recent activity (stack)";
        case Screen::HoldLists:        return "hold lists (queue)";
    }
    return "unknown";
}

std::vector<Screen> allScreens() {
    return {
        Screen::SearchResults,
        Screen::UnavailableTitle,
        Screen::Recommendations,
        Screen::AccountLoans,
        Screen::OverdueReport,
        Screen::InventoryTotals,
        Screen::RecentActivity,
        Screen::HoldLists
    };
}

// ---------------------------------------------------------------------------
// Catalog and accounts, shared by the fixtures that need them.
// ---------------------------------------------------------------------------
static void seedCatalogAndAccounts(LibrarySystem& lib) {
    lib.books().insert(Book("9780262134729", "Digital Apollo",
                            "Mindell",  "History",   3));
    lib.books().insert(Book("9780262529662", "The Apollo Guidance Computer",
                            "O'Brien",  "History",   1));
    lib.books().insert(Book("9780134685991", "Effective Modern C++",
                            "Meyers",   "Computing", 2));
    lib.books().insert(Book("9780201633610", "Design Patterns",
                            "Gamma",    "Computing", 4));
    lib.books().insert(Book("9780553380163", "A Brief History of Time",
                            "Hawking",  "Science",   2));
    lib.books().insert(Book("9780307887894", "The Lean Startup",
                            "Ries",     "Business",  2));

    lib.accounts().addUser(User(1001, "Mike",      "mike@example.edu",   "hash", Role::STUDENT));
    lib.accounts().addUser(User(1002, "Priya",     "priya@example.edu",  "hash", Role::STUDENT));
    lib.accounts().addUser(User(1003, "Sam",       "sam@example.edu",    "hash", Role::STUDENT));
    lib.accounts().addUser(User(2001, "A. Rivera", "rivera@example.edu", "hash", Role::LIBRARIAN));
    lib.accounts().addUser(User(3001, "Admin",     "admin@example.edu",  "hash", Role::ADMINISTRATOR));
}

// ---------------------------------------------------------------------------
// buildDemoLibrary: catalog, accounts, and a borrowing history.
//
// The history is the part that matters. Without prior loans the recommender
// has no co-borrow edges to walk and the overdue report has nothing to list,
// so two screens render empty while every function behind them works.
//
// Everything is deterministic: the same seed produces the same screens on
// every run, which is what makes the demo rehearsable and this fixture
// testable.
// ---------------------------------------------------------------------------
void buildDemoLibrary(LibrarySystem& lib) {
    seedCatalogAndAccounts(lib);

    // Priya and Sam both read the two Apollo-adjacent titles, which builds
    // the co-borrow edges the recommender walks for a patron holding one.
    lib.setToday(40);
    lib.checkout(1002, "9780262134729");   // Priya: Digital Apollo
    lib.checkout(1002, "9780553380163");   // Priya: A Brief History of Time
    lib.returnBook(1002, "9780262134729");
    lib.returnBook(1002, "9780553380163");

    lib.setToday(55);
    lib.checkout(1003, "9780262134729");   // Sam: Digital Apollo
    lib.checkout(1003, "9780553380163");   // Sam: A Brief History of Time
    lib.checkout(1003, "9780134685991");   // Sam: Effective Modern C++
    lib.returnBook(1003, "9780262134729");
    lib.returnBook(1003, "9780553380163");
    lib.returnBook(1003, "9780134685991");

    // Loans left outstanding on purpose. Due on days 91 and 96 against a
    // present day of 100, so the overdue report shows 9 and 4 days late.
    lib.setToday(70);
    lib.checkout(1002, "9780201633610");   // Priya: Design Patterns, due 91

    lib.setToday(75);
    lib.checkout(1003, "9780307887894");   // Sam: The Lean Startup, due 96

    // Single-copy title held by another patron. This is what lets the demo
    // show a refusal with a specific reason rather than a generic error.
    // Due day 111, so it is not overdue.
    lib.setToday(90);
    lib.checkout(1002, demo::UNAVAILABLE_ISBN);

    lib.setToday(100);                     // present day
}

void buildEmptyHistoryLibrary(LibrarySystem& lib) {
    seedCatalogAndAccounts(lib);
    lib.setToday(100);
}

void buildEmptyLibrary(LibrarySystem& lib) {
    lib.setToday(100);
}

// ---------------------------------------------------------------------------
// Screen inspection helpers. Each mirrors what the corresponding interface
// screen counts before drawing, using only the public LibrarySystem API.
// ---------------------------------------------------------------------------
static std::string lowered(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Substring match across title and author, the same rule searchCatalog uses.
static size_t searchMatchCount(LibrarySystem& lib, const std::string& term) {
    const std::string needle = lowered(term);
    size_t hits = 0;
    for (const Book& b : lib.books().getAll()) {
        if (lowered(b.title).find(needle)  != std::string::npos ||
            lowered(b.author).find(needle) != std::string::npos) {
            ++hits;
        }
    }
    return hits;
}

static size_t unavailableTitleCount(LibrarySystem& lib) {
    size_t out = 0;
    for (const Book& b : lib.books().getAll()) {
        if (b.copiesAvailable == 0) ++out;
    }
    return out;
}

static size_t activeLoanCount(LibrarySystem& lib, int userId) {
    User* u = lib.accounts().findUser(userId);
    return (u == nullptr) ? 0 : u->activeLoanCount();
}

// ---------------------------------------------------------------------------
// screenCoverage: replay the scripted path, then measure every screen.
// ---------------------------------------------------------------------------
std::vector<ScreenState> screenCoverage(LibrarySystem& lib) {
    // The scripted demo path. Without the borrow, the account, recommendation
    // and activity screens are legitimately empty, because the patron has not
    // done anything yet. Without the reservation, the hold list is empty for
    // the same reason. Both are actions the demo actually performs, so the
    // coverage check performs them too.
    lib.checkout(demo::PATRON_ID, demo::BORROWED_ISBN);
    lib.reserve(demo::PATRON_ID, demo::UNAVAILABLE_ISBN);

    const size_t searchHits   = searchMatchCount(lib, demo::SEARCH_TERM);
    const size_t unavailable  = unavailableTitleCount(lib);
    const size_t recCount     = lib.recommendationsFor(demo::PATRON_ID, 5).size();
    const size_t loanCount    = activeLoanCount(lib, demo::PATRON_ID);
    const size_t overdueCount = lib.overdueNotices().size();
    const size_t catalogSize  = lib.books().size();
    const size_t activityDepth= lib.activity().depth(demo::PATRON_ID);
    const size_t holdCount    = lib.reservations().totalWaiting();

    std::vector<ScreenState> out;
    for (Screen s : allScreens()) {
        size_t n = 0;
        switch (s) {
            case Screen::SearchResults:    n = searchHits;   break;
            case Screen::UnavailableTitle: n = unavailable;  break;
            case Screen::Recommendations:  n = recCount;     break;
            case Screen::AccountLoans:     n = loanCount;    break;
            case Screen::OverdueReport:    n = overdueCount; break;
            case Screen::InventoryTotals:  n = catalogSize;  break;
            case Screen::RecentActivity:   n = activityDepth;break;
            case Screen::HoldLists:        n = holdCount;    break;
        }
        out.push_back(ScreenState{s, n > 0, n});
    }
    return out;
}

bool allScreensPopulated(LibrarySystem& lib) {
    for (const ScreenState& st : screenCoverage(lib)) {
        if (!st.populated) return false;
    }
    return true;
}
