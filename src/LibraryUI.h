// ============================================================================
// LibraryUI.h
// Smart Library Management System - CPSC 315
//
// Console interface. Separated from LibrarySystem so the logic can be tested
// without simulating keystrokes, and so the interface can be replaced later
// without touching the data structures.
//
// Design rules applied during Module 7 refinement:
//   1. Numbered options only. The user never has to remember a command word.
//   2. Every screen shows who is signed in and their role, so a shared
//      terminal never leaves ambiguity about who is acting.
//   3. Menus are grouped by frequency, not by module. The operations a
//      patron performs most often appear first; administrative functions sit
//      below a separator and are hidden entirely from roles that cannot use
//      them.
//   4. Every rejection prints the specific rule that stopped it, sourced from
//      the Status value the logic layer returns, never a generic failure.
//   5. Invalid input re-prompts rather than exiting. A stray keystroke should
//      never end a session.
// ============================================================================
#ifndef LIBRARY_UI_H
#define LIBRARY_UI_H

#include "LibrarySystem.h"
#include <iostream>
#include <string>

class LibraryUI {
public:
    explicit LibraryUI(LibrarySystem& system);

    // Runs the session loop until the user signs out and exits.
    void run();

    // Loads a small demo catalog and set of accounts so the interface can be
    // exercised without manual data entry.
    void seedDemoData();

private:
    LibrarySystem& lib;
    int  currentUserId;      // 0 when nobody is signed in

    // --- screens ---
    bool signInScreen();     // false if the user chose to quit
    void mainMenu();
    void searchCatalog();
    void borrowFlow();
    void returnFlow();
    void myAccount();
    void recommendations();
    void inventoryReport();  // librarian and administrator only
    void overdueReport();    // librarian and administrator only

    // --- helpers ---
    void header(const std::string& screenName) const;
    void divider() const;
    void pause() const;

    // Reads a menu choice, re-prompting until the input is a number inside
    // the allowed range. Returns -1 if the input stream closes.
    int  readChoice(int low, int high) const;
    std::string readLine(const std::string& prompt) const;

    Role currentRole() const;
    bool isStaff() const;
};

#endif // LIBRARY_UI_H
