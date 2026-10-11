// DemoFixture.h - seed data, the dynamic screen list, and a coverage check.
//
// The fixture used to live inside LibraryUI, which the test driver does not
// link, so no test could see it. That is how the Module 8 dry run ended up
// with two empty screens while every test passed. Each fixture below declares
// the interface state it exists to produce, and screenCoverage verifies it
// reaches it.

#ifndef DEMO_FIXTURE_H
#define DEMO_FIXTURE_H

#include "LibrarySystem.h"
#include <string>
#include <vector>

// Screens whose content depends on system state, so they can render empty
// while the code behind them is correct. Menus are not listed: data cannot
// empty them.
enum class Screen {
    SearchResults,      // catalog search returns at least one match
    UnavailableTitle,   // a title with every copy out, so a refusal can occur
    Recommendations,    // the co-borrow walk produces at least one suggestion
    AccountLoans,       // the signed-in patron holds at least one active loan
    OverdueReport,      // at least one loan is past its due date
    InventoryTotals,    // the catalog is non-empty, so totals are meaningful
    RecentActivity,     // the patron's activity stack has something on it
    HoldLists           // at least one title has someone waiting
};

const char* screenName(Screen s);

// Every screen above, in declaration order. The coverage check walks this
// list rather than a hand-written set, so adding a Screen value without
// extending the check is not possible.
std::vector<Screen> allScreens();

// ---------------------------------------------------------------------------
// The scripted demo path. These are the exact values demo/demo_input.txt
// feeds the interface, kept here so the fixture and the demo cannot drift
// apart silently.
// ---------------------------------------------------------------------------
namespace demo {
    constexpr int  PATRON_ID       = 1001;              // signs in first
    constexpr int  LIBRARIAN_ID    = 2001;              // signs in for reports
    extern const std::string SEARCH_TERM;               // typed into search
    extern const std::string BORROWED_ISBN;             // borrowed in scene 1
    extern const std::string UNAVAILABLE_ISBN;          // refused in scene 2
    extern const std::string PASSWORD;                  // every demo account
}

// ---------------------------------------------------------------------------
// Fixtures. Each one states the interface state it exists to produce.
// ---------------------------------------------------------------------------

// Catalog, accounts, and a borrowing history. Targets the populated state of
// every screen in allScreens(). This is what the application runs on.
void buildDemoLibrary(LibrarySystem& lib);

// Catalog and accounts with no borrowing activity at all. Targets the empty
// state of Recommendations, AccountLoans, and OverdueReport. This reproduces
// the Module 8 defect deliberately, so the empty rendering stays covered
// rather than being tested by accident.
void buildEmptyHistoryLibrary(LibrarySystem& lib);

// Nothing at all. Targets the empty state of every screen, including the
// catalog-level ones that buildEmptyHistoryLibrary still populates.
void buildEmptyLibrary(LibrarySystem& lib);

// Replays the scripted demo path, then reports whether each screen has
// content. The replay matters: several screens are legitimately empty until
// the patron acts, so checking the fixture at rest would report false
// failures. Mutates the system it is given; the application never calls it.
struct ScreenState {
    Screen screen;
    bool   populated;
    size_t itemCount;       // rows the screen would draw
};

std::vector<ScreenState> screenCoverage(LibrarySystem& lib);

// Convenience: true when every screen in allScreens() is populated.
bool allScreensPopulated(LibrarySystem& lib);

#endif // DEMO_FIXTURE_H
