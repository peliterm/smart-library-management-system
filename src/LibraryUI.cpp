// ============================================================================
// LibraryUI.cpp
// Smart Library Management System - CPSC 315
// ============================================================================
#include "LibraryUI.h"
#include <iomanip>
#include <limits>
#include <sstream>
#include <algorithm>

LibraryUI::LibraryUI(LibrarySystem& system)
    : lib(system), currentUserId(0) {}

// ---------------------------------------------------------------------------
// Display helpers
// ---------------------------------------------------------------------------
void LibraryUI::divider() const {
    std::cout << "--------------------------------------------------------\n";
}

void LibraryUI::header(const std::string& screenName) const {
    std::cout << "\n";
    divider();
    std::cout << " " << std::left << std::setw(36) << screenName;

    if (currentUserId != 0) {
        User* u = const_cast<LibrarySystem&>(lib).accounts().findUser(currentUserId);
        if (u != nullptr) {
            std::string who = u->name + " / " + roleName(u->role);
            std::cout << std::right << std::setw(18) << who;
        }
    }
    std::cout << "\n";
    divider();
}

void LibraryUI::pause() const {
    std::cout << "\nPress Enter to continue.";
    std::string discard;
    std::getline(std::cin, discard);
}

// ---------------------------------------------------------------------------
// readChoice: re-prompts on anything that is not a number in range. An
// invalid keystroke never ends the session, which was the single most common
// complaint during Module 7 walkthrough testing.
// ---------------------------------------------------------------------------
int LibraryUI::readChoice(int low, int high) const {
    std::string line;
    while (true) {
        std::cout << "Select [" << low << "-" << high << "]: ";
        if (!std::getline(std::cin, line)) return -1;   // stream closed

        std::istringstream iss(line);
        int value = 0;
        char leftover = 0;
        if ((iss >> value) && !(iss >> leftover) && value >= low && value <= high) {
            return value;
        }
        std::cout << "  That is not one of the listed options. "
                  << "Enter a number between " << low << " and " << high << ".\n";
    }
}

std::string LibraryUI::readLine(const std::string& prompt) const {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) return "";
    return line;
}

Role LibraryUI::currentRole() const {
    User* u = const_cast<LibrarySystem&>(lib).accounts().findUser(currentUserId);
    return (u != nullptr) ? u->role : Role::STUDENT;
}

bool LibraryUI::isStaff() const {
    Role r = currentRole();
    return r == Role::LIBRARIAN || r == Role::ADMINISTRATOR;
}

// ---------------------------------------------------------------------------
// seedDemoData: a catalog, accounts, and a borrowing history.
//
// The history matters for the demo. Without prior loans the recommendation
// engine has no co-borrow edges to walk and the overdue report is empty, so
// two of the system's features would demo as blank screens. Seeding real
// activity and then advancing the clock makes both produce output.
//
// Everything here is deterministic: the same seed produces the same screens
// on every run, which is what makes the demo rehearsable.
// ---------------------------------------------------------------------------
void LibraryUI::seedDemoData() {
    // --- catalog ---
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

    // --- accounts ---
    lib.accounts().addUser(User(1001, "Mike",      "mike@example.edu",   "hash", Role::STUDENT));
    lib.accounts().addUser(User(1002, "Priya",     "priya@example.edu",  "hash", Role::STUDENT));
    lib.accounts().addUser(User(1003, "Sam",       "sam@example.edu",    "hash", Role::STUDENT));
    lib.accounts().addUser(User(2001, "A. Rivera", "rivera@example.edu", "hash", Role::LIBRARIAN));
    lib.accounts().addUser(User(3001, "Admin",     "admin@example.edu",  "hash", Role::ADMINISTRATOR));

    // --- borrowing history ---
    // Priya and Sam both read the two Apollo titles, which builds the
    // co-borrow edge the recommender walks for a user holding one of them.
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

    // --- loans still outstanding, deliberately left overdue ---
    // Due on day 91 and 96; the clock below sits at 100, so the overdue
    // report has two entries at 9 and 4 days late.
    lib.setToday(70);
    lib.checkout(1002, "9780201633610");   // Priya: Design Patterns, due day 91

    lib.setToday(75);
    lib.checkout(1003, "9780307887894");   // Sam: The Lean Startup, due day 96

    // Single-copy title held by another patron. This is what lets the demo
    // show a refusal with a specific reason rather than a generic error:
    // the search screen reports "all copies out" and the borrow attempt
    // returns NO_COPIES_AVAILABLE. Due day 111, so it is not overdue.
    lib.setToday(90);
    lib.checkout(1002, "9780262529662");   // Priya: Apollo Guidance Computer

    // --- present day ---
    lib.setToday(100);
}

// ---------------------------------------------------------------------------
// Session loop
// ---------------------------------------------------------------------------
void LibraryUI::run() {
    std::cout << "\n";
    divider();
    std::cout << " SMART LIBRARY MANAGEMENT SYSTEM\n";
    divider();

    while (true) {
        if (!signInScreen()) break;
        mainMenu();
        currentUserId = 0;      // sign out returns here
    }
    std::cout << "\nSession ended.\n";
}

bool LibraryUI::signInScreen() {
    header("SIGN IN");
    std::cout << " Demo accounts:\n"
              << "   1001  Mike       (Student)\n"
              << "   1002  Priya      (Student)\n"
              << "   2001  A. Rivera  (Librarian)\n"
              << "   3001  Admin      (Administrator)\n\n"
              << " Enter 0 to exit.\n\n";

    while (true) {
        std::string line = readLine("Account ID: ");
        if (line.empty() && !std::cin) return false;

        std::istringstream iss(line);
        int id = -1;
        if (!(iss >> id)) {
            std::cout << "  Account IDs are numbers. Try again.\n";
            continue;
        }
        if (id == 0) return false;

        if (lib.accounts().findUser(id) == nullptr) {
            std::cout << "  No account found with ID " << id
                      << ". Check the list above.\n";
            continue;
        }
        currentUserId = id;
        return true;
    }
}

// ---------------------------------------------------------------------------
// mainMenu: options are ordered by how often they are used. Staff functions
// appear below a separator and only for roles that can use them, so a
// student never sees an option that would be refused.
// ---------------------------------------------------------------------------
void LibraryUI::mainMenu() {
    while (true) {
        header("MAIN MENU");
        std::cout << "   1. Search and borrow\n"
                  << "   2. Borrow by ISBN\n"
                  << "   3. Return a book\n"
                  << "   4. My account\n"
                  << "   5. Recommendations for me\n";

        int high = 6;
        if (isStaff()) {
            std::cout << "\n   Staff functions\n"
                      << "   6. Overdue report\n"
                      << "   7. Inventory report\n"
                      << "\n   8. Sign out\n";
            high = 8;
        } else {
            std::cout << "\n   6. Sign out\n";
        }
        std::cout << "\n";

        int choice = readChoice(1, high);
        if (choice == -1) return;

        if (!isStaff() && choice == 6) return;      // student sign out
        if (isStaff()  && choice == 8) return;      // staff sign out

        switch (choice) {
            case 1: searchCatalog();   break;
            case 2: borrowFlow();      break;
            case 3: returnFlow();      break;
            case 4: myAccount();       break;
            case 5: recommendations(); break;
            case 6: overdueReport();   break;   // staff only, guarded above
            case 7: inventoryReport(); break;   // staff only
            default: break;
        }
    }
}

// ---------------------------------------------------------------------------
// searchCatalog: substring match on title and author, case-insensitive.
//
// REFINEMENT (Module 7): results are numbered and can be borrowed directly
// from this screen. The original version displayed results and sent the user
// back to the menu to type a 13-digit ISBN by hand. Writing that step into
// the user manual is what exposed it: any instruction that reads "type the
// number exactly as shown above" is describing work the program should be
// doing. Manual ISBN entry remains available for staff working from a
// printed list.
// ---------------------------------------------------------------------------
void LibraryUI::searchCatalog() {
    header("SEARCH CATALOG");
    std::string query = readLine(" Title or author (blank lists everything): ");

    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    std::vector<std::string> all = lib.isbnsSortedByTitle();
    std::vector<std::string> matches;      // holds the numbered results

    for (const std::string& isbn : all) {
        Book* b = lib.books().find(isbn);
        if (b == nullptr) continue;

        std::string haystack = b->title + " " + b->author;
        std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (lowerQuery.empty() || haystack.find(lowerQuery) != std::string::npos) {
            matches.push_back(isbn);
        }
    }

    std::cout << "\n";
    if (matches.empty()) {
        std::cout << "   No titles matched \"" << query << "\".\n"
                  << "   Try a shorter search term, or leave it blank to see"
                  << " the whole catalog.\n";
        pause();
        return;
    }

    for (size_t i = 0; i < matches.size(); ++i) {
        Book* b = lib.books().find(matches[i]);
        std::cout << "   " << std::right << std::setw(2) << (i + 1) << ". "
                  << std::left << std::setw(32) << b->title.substr(0, 31)
                  << std::setw(12) << b->author.substr(0, 11)
                  << (b->isAvailable()
                        ? std::to_string(b->copiesAvailable) + " available"
                        : "all copies out")
                  << "\n";
    }

    std::cout << "\n   " << matches.size() << " title"
              << (matches.size() == 1 ? "" : "s") << " found.\n"
              << "   " << (matches.size() + 1) << ". Back to menu\n\n";

    int choice = readChoice(1, static_cast<int>(matches.size()) + 1);
    if (choice == -1 || choice == static_cast<int>(matches.size()) + 1) return;

    // Borrow the selected title without retyping anything.
    const std::string& isbn = matches[choice - 1];
    int loanId = 0;
    Status s = lib.checkout(currentUserId, isbn, &loanId);

    std::cout << "\n";
    if (s == Status::OK) {
        Book* b = lib.books().find(isbn);
        std::cout << "   Checked out: " << (b ? b->title : isbn) << "\n"
                  << "   Due in " << LibrarySystem::LOAN_PERIOD_DAYS
                  << " days (day " << lib.today() + LibrarySystem::LOAN_PERIOD_DAYS
                  << ").\n";
    } else {
        std::cout << "   Not checked out. " << statusMessage(s) << "\n";
    }
    pause();
}

void LibraryUI::borrowFlow() {
    header("BORROW A BOOK");
    std::string isbn = readLine(" ISBN (blank to cancel): ");
    if (isbn.empty()) return;

    int loanId = 0;
    Status s = lib.checkout(currentUserId, isbn, &loanId);

    std::cout << "\n";
    if (s == Status::OK) {
        Book* b = lib.books().find(isbn);
        std::cout << "   Checked out: " << (b ? b->title : isbn) << "\n"
                  << "   Due in " << LibrarySystem::LOAN_PERIOD_DAYS
                  << " days (day " << lib.today() + LibrarySystem::LOAN_PERIOD_DAYS
                  << ").\n";
    } else {
        // The specific rule that stopped it, not a generic failure.
        std::cout << "   Not checked out. " << statusMessage(s) << "\n";
    }
    pause();
}

void LibraryUI::returnFlow() {
    header("RETURN A BOOK");

    User* u = lib.accounts().findUser(currentUserId);
    if (u == nullptr || u->borrowedIsbns.empty()) {
        std::cout << "\n   You have no books checked out.\n";
        pause();
        return;
    }

    std::cout << " Your current loans:\n";
    for (size_t i = 0; i < u->borrowedIsbns.size(); ++i) {
        Book* b = lib.books().find(u->borrowedIsbns[i]);
        std::cout << "   " << (i + 1) << ". "
                  << (b ? b->title : u->borrowedIsbns[i])
                  << "  (" << u->borrowedIsbns[i] << ")\n";
    }
    std::cout << "   " << (u->borrowedIsbns.size() + 1) << ". Cancel\n\n";

    int choice = readChoice(1, static_cast<int>(u->borrowedIsbns.size()) + 1);
    if (choice == -1 || choice == static_cast<int>(u->borrowedIsbns.size()) + 1) return;

    std::string isbn = u->borrowedIsbns[choice - 1];
    Status s = lib.returnBook(currentUserId, isbn);

    std::cout << "\n";
    if (s == Status::OK) {
        std::cout << "   Returned. Thank you.\n";
    } else {
        std::cout << "   Not returned. " << statusMessage(s) << "\n";
    }
    pause();
}

void LibraryUI::myAccount() {
    header("MY ACCOUNT");

    User* u = lib.accounts().findUser(currentUserId);
    if (u == nullptr) { pause(); return; }

    std::cout << " Account " << u->userId << "  " << u->name
              << "  (" << roleName(u->role) << ")\n"
              << " Active loans: " << u->activeLoanCount()
              << " of " << LibrarySystem::MAX_ACTIVE_LOANS << "\n\n";

    if (u->borrowedIsbns.empty()) {
        std::cout << "   Nothing checked out.\n";
    } else {
        for (const std::string& isbn : u->borrowedIsbns) {
            Book* b = lib.books().find(isbn);
            std::cout << "   " << std::left << std::setw(36)
                      << (b ? b->title.substr(0, 35) : isbn) << isbn << "\n";
        }
    }
    pause();
}

void LibraryUI::recommendations() {
    header("RECOMMENDED FOR YOU");

    std::vector<Recommendation> recs = lib.recommendationsFor(currentUserId, 5);
    std::cout << "\n";

    if (recs.empty()) {
        // Explains why the list is empty rather than showing nothing.
        std::cout << "   No recommendations yet. Suggestions are built from\n"
                  << "   titles borrowed by readers who borrowed the same\n"
                  << "   books you have. Check out a few books and come back.\n";
    } else {
        for (const Recommendation& r : recs) {
            Book* b = lib.books().find(r.isbn);
            std::cout << "   " << std::left << std::setw(36)
                      << (b ? b->title.substr(0, 35) : r.isbn)
                      << "match score " << r.score << "\n";
        }
    }
    pause();
}

void LibraryUI::overdueReport() {
    header("OVERDUE REPORT");

    std::vector<OverdueNotice> notices = lib.overdueNotices();
    std::cout << " As of day " << lib.today() << "\n\n";

    if (notices.empty()) {
        std::cout << "   Nothing overdue.\n";
    } else {
        for (const OverdueNotice& n : notices) {
            std::cout << "   " << std::left << std::setw(30)
                      << n.title.substr(0, 29)
                      << std::setw(14) << n.userName
                      << n.daysLate << " day" << (n.daysLate == 1 ? "" : "s")
                      << " late\n";
        }
        std::cout << "\n   " << notices.size() << " overdue item"
                  << (notices.size() == 1 ? "" : "s") << ".\n";
    }
    pause();
}

void LibraryUI::inventoryReport() {
    header("INVENTORY REPORT");

    std::vector<std::string> isbns = lib.isbnsSortedByAuthor();
    int totalCopies = 0, outCopies = 0;

    std::cout << "\n";
    for (const std::string& isbn : isbns) {
        Book* b = lib.books().find(isbn);
        if (b == nullptr) continue;

        totalCopies += b->copiesTotal;
        outCopies   += (b->copiesTotal - b->copiesAvailable);

        std::cout << "   " << std::left << std::setw(14) << b->author.substr(0, 13)
                  << std::setw(32) << b->title.substr(0, 31)
                  << b->copiesAvailable << " of " << b->copiesTotal << "\n";
    }

    std::cout << "\n   " << isbns.size() << " titles, " << totalCopies
              << " copies, " << outCopies << " currently out.\n"
              << "   Active loans tracked: " << lib.deadlines().size() << "\n";
    pause();
}
