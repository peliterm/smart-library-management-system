// ============================================================================
// DemoFixture.h
// Smart Library Management System - CPSC 315
//
// Module 9 addition. The seed data used to run the system was previously
// written inside LibraryUI, which meant the test driver could not see it.
// That gap caused a real failure: during the Module 8 dry run, two screens
// rendered empty because the seed contained a catalog and accounts but no
// borrowing history. Every underlying operation worked. Nothing was broken.
// There was simply nothing for those screens to display, and no test had an
// opinion about that, because the fixture lived where tests could not reach.
//
// Moving the fixture here makes it a tested artifact rather than a constant
// buried in the interface layer. Both the application and the test driver
// build their data from the same functions, so a change that empties a screen
// fails the build instead of surfacing during a rehearsal.
//
// The design follows from one idea: every dynamic screen has at least two
// states, populated and empty, and a fixture is only correct with respect to
// the state it is meant to produce. So the screens are enumerated, each
// fixture declares which state it targets, and the coverage check verifies
// the fixture actually reaches it.
// ============================================================================
#ifndef DEMO_FIXTURE_H
#define DEMO_FIXTURE_H

#include "LibrarySystem.h"
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// The dynamic screens. "Dynamic" means the content depends on system state
// rather than on static layout, so the screen can render empty even when the
// code behind it is correct. Static screens (the sign-in prompt, the menus)
// are not listed because they cannot be emptied by a change to the data.
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Coverage check.
//
// Replays the scripted demo path against whatever fixture is already loaded,
// then reports whether each screen has content. The replay matters: the
// account and recommendation screens are empty for the demo patron until the
// scripted checkout in scene 1 runs, so checking the fixture at rest would
// report a false failure. The check measures what the audience sees, not what
// the fixture looks like before anyone touches it.
//
// This mutates the system it is given, so tests pass it their own instance.
// The application never calls it.
// ---------------------------------------------------------------------------
struct ScreenState {
    Screen screen;
    bool   populated;
    size_t itemCount;       // rows the screen would draw
};

std::vector<ScreenState> screenCoverage(LibrarySystem& lib);

// Convenience: true when every screen in allScreens() is populated.
bool allScreensPopulated(LibrarySystem& lib);

#endif // DEMO_FIXTURE_H
