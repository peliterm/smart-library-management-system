// LibraryUI.h - console interface. Holds no data of its own and makes no
// policy decisions; every rule it enforces is enforced again in
// LibrarySystem, so a second front end could not bypass one.

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
    void reserveFlow();      // join a hold line (queue)
    void activityFlow();     // recent activity, with undo (stack)
    void inventoryReport();  // librarian and administrator only
    void overdueReport();    // librarian and administrator only
    void holdsReport();      // librarian and administrator only
    void storageReport();    // librarian and administrator only
    void manageFlow();       // librarian and administrator only

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
