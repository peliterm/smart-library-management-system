// LibraryUI.cpp

#include "LibraryUI.h"
#include "DemoFixture.h"
#include "Huffman.h"
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

// Delegates to the shared fixture, which the test driver also links.
void LibraryUI::seedDemoData() {
    buildDemoLibrary(lib);
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
              << " Password for every demo account: library\n\n"
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

        User* account = lib.accounts().findUser(id);
        if (account == nullptr) {
            std::cout << "  No account found with ID " << id
                      << ". Check the list above.\n";
            continue;
        }

        // Password check. Until Module 9 this screen took an account ID and
        // nothing else, so anyone could type the administrator's ID straight
        // off the demo list. The stored field held a placeholder that nothing
        // ever read.
        std::string password = readLine("Password:   ");
        if (!lib.authenticate(id, password)) {
            std::cout << "  That password does not match the account.\n";
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
                  << "   5. Recommendations for me\n"
                  << "   6. Reserve a title that is out\n"
                  << "   7. My recent activity\n";

        int high = 8;
        if (isStaff()) {
            std::cout << "\n   Staff functions\n"
                      << "    8. Overdue report\n"
                      << "    9. Inventory report\n"
                      << "   10. Hold lists\n"
                      << "   11. Storage report\n"
                      << "   12. Manage catalog and accounts\n"
                      << "\n   13. Sign out\n";
            high = 13;
        } else {
            std::cout << "\n   8. Sign out\n";
        }
        std::cout << "\n";

        int choice = readChoice(1, high);
        if (choice == -1) return;

        if (!isStaff() && choice == 8)  return;     // student sign out
        if (isStaff()  && choice == 13) return;     // staff sign out

        switch (choice) {
            case 1:  searchCatalog();   break;
            case 2:  borrowFlow();      break;
            case 3:  returnFlow();      break;
            case 4:  myAccount();       break;
            case 5:  recommendations(); break;
            case 6:  reserveFlow();     break;
            case 7:  activityFlow();    break;
            case 8:  overdueReport();   break;   // staff only, guarded above
            case 9:  inventoryReport(); break;   // staff only
            case 10: holdsReport();     break;   // staff only
            case 11: storageReport();   break;   // staff only
            case 12: manageFlow();      break;   // staff only
            default: break;
        }
    }
}

// Join a hold line. Only titles with every copy out are listed: reserving an
// available book is refused, and offering it would invite a rejection.
void LibraryUI::reserveFlow() {
    header("RESERVE A TITLE");

    std::vector<Book> all = lib.books().getAll();
    std::vector<Book> out;
    for (const Book& b : all) {
        if (b.copiesAvailable == 0) out.push_back(b);
    }

    if (out.empty()) {
        std::cout << "\n   Every title has a copy on the shelf.\n"
                  << "   Nothing needs reserving right now.\n";
        pause();
        return;
    }

    std::sort(out.begin(), out.end(),
              [](const Book& a, const Book& b) { return a.title < b.title; });

    std::cout << " Titles with every copy checked out:\n\n";
    for (size_t i = 0; i < out.size(); ++i) {
        size_t waiting = lib.reservations().lengthOf(out[i].isbn);
        std::cout << "   " << (i + 1) << ". " << std::left << std::setw(34)
                  << out[i].title.substr(0, 33);
        if (waiting == 0) {
            std::cout << "no one waiting\n";
        } else {
            std::cout << waiting << " waiting\n";
        }
    }
    std::cout << "   " << (out.size() + 1) << ". Cancel\n\n";

    int choice = readChoice(1, static_cast<int>(out.size()) + 1);
    if (choice == -1 || choice == static_cast<int>(out.size()) + 1) return;

    const std::string isbn = out[choice - 1].isbn;
    Status s = lib.reserve(currentUserId, isbn);

    std::cout << "\n";
    if (s == Status::OK) {
        size_t place = lib.reservations().lengthOf(isbn);
        std::cout << "   Reserved. You are number " << place
                  << " in line.\n"
                  << "   The next copy returned goes to the front of the line.\n";
    } else {
        std::cout << "   Not reserved. " << statusMessage(s) << "\n";
    }
    pause();
}

// The account's activity stack, newest first, with an undo for the newest.
void LibraryUI::activityFlow() {
    header("MY RECENT ACTIVITY");

    std::vector<ActivityEntry> recent = lib.activity().recent(currentUserId, 8);

    if (recent.empty()) {
        std::cout << "\n   Nothing recorded on this account yet.\n";
        pause();
        return;
    }

    std::cout << " Most recent first. Day numbers are system days.\n\n";
    for (size_t i = 0; i < recent.size(); ++i) {
        Book* b = lib.books().find(recent[i].isbn);
        std::cout << "   " << (i + 1) << ". " << std::left << std::setw(22)
                  << activityName(recent[i].type) << std::setw(32)
                  << (b ? b->title.substr(0, 31) : recent[i].isbn)
                  << "day " << recent[i].day << "\n";
    }

    std::cout << "\n   Only the most recent action can be undone.\n"
              << "   1. Undo \"" << activityName(recent[0].type) << "\"\n"
              << "   2. Back\n\n";

    int choice = readChoice(1, 2);
    if (choice != 1) return;

    ActivityEntry undone;
    Status s = lib.undoLast(currentUserId, &undone);

    std::cout << "\n";
    if (s == Status::OK) {
        Book* b = lib.books().find(undone.isbn);
        std::cout << "   Undone: " << activityName(undone.type) << " "
                  << (b ? b->title : undone.isbn) << "\n";
    } else {
        std::cout << "   Not undone. " << statusMessage(s) << "\n";
    }
    pause();
}

// Staff add and remove for books and accounts. Every operation goes through a
// guarded LibrarySystem call, so the role check does not depend on this screen
// being unreachable.
void LibraryUI::manageFlow() {
    while (true) {
        header("MANAGE CATALOG AND ACCOUNTS");

        if (!lib.isStaff(currentUserId)) {
            std::cout << "\n   "
                      << statusMessage(Status::NOT_AUTHORIZED) << "\n";
            pause();
            return;
        }

        std::cout << "   1. Add a book\n"
                  << "   2. Remove a book\n"
                  << "   3. Add an account\n"
                  << "   4. Remove an account\n"
                  << "   5. Back\n\n";

        int choice = readChoice(1, 5);
        if (choice == -1 || choice == 5) return;

        std::cout << "\n";
        if (choice == 1) {
            std::string isbn   = readLine(" ISBN:            ");
            std::string title  = readLine(" Title:           ");
            std::string author = readLine(" Author:          ");
            std::string genre  = readLine(" Genre:           ");
            std::string copies = readLine(" Copies:          ");
            std::string year   = readLine(" Year (0 if n/a): ");

            int copyCount = 0, pubYear = 0;
            std::istringstream(copies) >> copyCount;
            std::istringstream(year)   >> pubYear;

            if (isbn.empty() || title.empty() || copyCount <= 0) {
                std::cout << "\n   An ISBN, a title, and at least one copy are "
                          << "required.\n";
            } else {
                Status s = lib.addBook(currentUserId,
                                       Book(isbn, title, author, genre,
                                            copyCount, pubYear));
                std::cout << "\n   "
                          << (s == Status::OK ? "Added." : statusMessage(s))
                          << "\n";
            }

        } else if (choice == 2) {
            std::string isbn = readLine(" ISBN to remove: ");
            Status s = lib.removeBook(currentUserId, isbn);
            std::cout << "\n   "
                      << (s == Status::OK ? "Removed." : statusMessage(s))
                      << "\n";
            if (s == Status::STILL_IN_USE) {
                std::cout << "   Copies are out or patrons are waiting on it.\n";
            }

        } else if (choice == 3) {
            std::string idText = readLine(" Account ID:  ");
            std::string name   = readLine(" Name:        ");
            std::string email  = readLine(" Email:       ");
            std::string pw     = readLine(" Password:    ");
            std::string roleIn = readLine(" Role [1 student, 2 librarian, "
                                          "3 administrator]: ");

            int id = 0, roleNum = 1;
            std::istringstream(idText) >> id;
            std::istringstream(roleIn) >> roleNum;

            Role role = Role::STUDENT;
            if (roleNum == 2) role = Role::LIBRARIAN;
            if (roleNum == 3) role = Role::ADMINISTRATOR;

            if (id <= 0 || name.empty() || pw.empty()) {
                std::cout << "\n   An ID, a name, and a password are required.\n";
            } else {
                Status s = lib.addAccount(currentUserId,
                                          User(id, name, email, "", role), pw);
                std::cout << "\n   "
                          << (s == Status::OK ? "Account created." : statusMessage(s))
                          << "\n";
            }

        } else if (choice == 4) {
            std::string idText = readLine(" Account ID to remove: ");
            int id = 0;
            std::istringstream(idText) >> id;

            if (id == currentUserId) {
                std::cout << "\n   You cannot remove the account you are "
                          << "signed in to.\n";
            } else {
                Status s = lib.removeAccount(currentUserId, id);
                std::cout << "\n   "
                          << (s == Status::OK ? "Account removed." : statusMessage(s))
                          << "\n";
                if (s == Status::STILL_IN_USE) {
                    std::cout << "   The account still holds loans or is waiting "
                              << "on a title.\n";
                }
            }
        }
        pause();
    }
}

// What the catalog costs to store, and what Huffman saves. At demo scale the
// archive is larger than the source: the frequency table is a fixed cost that
// six books cannot amortize. The screen shows that rather than hiding it.
void LibraryUI::storageReport() {
    header("STORAGE REPORT");

    std::vector<std::string> allowed;
    Status ok = lib.inventoryReport(currentUserId, allowed);
    if (ok != Status::OK) {
        std::cout << "\n   " << statusMessage(ok) << "\n";
        pause();
        return;
    }

    const std::string exported = lib.catalogExport();
    huffman::Archive archive = huffman::encode(exported);
    const size_t header_ = archive.storedBytes() - archive.bytes.size();

    std::cout << " Catalog export, Huffman coded.\n\n"
              << "   Records                " << lib.books().size() << "\n"
              << "   Export size            " << exported.size() << " bytes\n"
              << "   Distinct symbols       " << archive.frequencies.size() << "\n"
              << "   Coded payload          " << archive.bytes.size() << " bytes\n"
              << "   Frequency table        " << header_ << " bytes\n"
              << "   Stored total           " << archive.storedBytes() << " bytes\n";

    std::cout << std::fixed << std::setprecision(2)
              << "   Ratio                  " << archive.ratio() << "\n\n";
    std::cout.unsetf(std::ios::fixed);

    const bool roundTrips = (huffman::decode(archive) == exported);
    std::cout << "   Round-trip check       "
              << (roundTrips ? "exact" : "FAILED") << "\n\n";

    if (archive.ratio() >= 1.0) {
        std::cout << "   The archive is larger than the export. The frequency\n"
                  << "   table costs five bytes per distinct symbol, which at\n"
                  << "   this catalog size outweighs what the coding saves.\n"
                  << "   The payload alone is " << archive.bytes.size()
                  << " bytes against " << exported.size() << ".\n";
    } else {
        std::cout << "   The table is amortized at this size and the archive\n"
                  << "   is smaller than the export.\n";
    }
    pause();
}

// Staff view of every title with a waiting list.
void LibraryUI::holdsReport() {
    header("HOLD LISTS");

    std::vector<std::string> titles;
    Status allowed = lib.holdReport(currentUserId, titles);
    if (allowed != Status::OK) {
        std::cout << "\n   " << statusMessage(allowed) << "\n";
        pause();
        return;
    }

    if (titles.empty()) {
        std::cout << "\n   No titles have anyone waiting.\n";
        pause();
        return;
    }

    std::sort(titles.begin(), titles.end());

    std::cout << " Waiting lists, in the order patrons will be served:\n\n";
    for (const std::string& isbn : titles) {
        Book* b = lib.books().find(isbn);
        std::cout << "   " << (b ? b->title : isbn) << "\n";

        std::vector<int> line = lib.reservations().waitingOn(isbn);
        for (size_t i = 0; i < line.size(); ++i) {
            User* u = lib.accounts().findUser(line[i]);
            std::cout << "      " << (i + 1) << ". "
                      << (u ? u->name : std::string("account "))
                      << " (" << line[i] << ")\n";
        }
        std::cout << "\n";
    }

    std::cout << " Total patrons waiting: "
              << lib.reservations().totalWaiting() << "\n";
    pause();
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

    // Search goes through the word index rather than scanning the catalog.
    // A blank query still lists everything, which is a browse rather than a
    // search and has nothing to look up.
    std::vector<std::string> matches;      // holds the numbered results

    if (query.empty()) {
        matches = lib.isbnsSortedByTitle();
    } else {
        matches = lib.books().search(query);

        // Results come back in index order, which is bucket order and means
        // nothing to a reader. Put them in the same title order the rest of
        // the interface uses.
        std::vector<std::string> ordered;
        for (const std::string& isbn : lib.isbnsSortedByTitle()) {
            if (std::find(matches.begin(), matches.end(), isbn) != matches.end()) {
                ordered.push_back(isbn);
            }
        }
        matches = ordered;
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
        // Due dates were in the Module 2 account mock-up and were missing
        // from the built screen until the Module 9 review. A patron cannot
        // act on a loan limit without knowing which loan expires first.
        std::cout << "   " << std::left << std::setw(34) << "Title"
                  << std::setw(10) << "Due" << "Status\n";

        // One row per loan, not per title. Two copies of one book are two
        // loans with two deadlines, and showing the earlier date against both
        // would tell the patron a copy is due sooner than it is.
        for (const LoanEntry& loan : lib.loansFor(u->userId)) {
            Book* b = lib.books().find(loan.isbn);
            std::cout << "   " << std::left << std::setw(34)
                      << (b ? b->title.substr(0, 33) : loan.isbn)
                      << std::setw(10)
                      << ("day " + std::to_string(loan.dueDay));

            const int late = lib.today() - loan.dueDay;
            if (late > 0) {
                std::cout << late << " day" << (late == 1 ? "" : "s")
                          << " OVERDUE";
            } else {
                std::cout << "due in " << -late << " day"
                          << (-late == 1 ? "" : "s");
            }
            std::cout << "\n";
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

    // The permission check lives in the logic layer. The menu already hides
    // this option from patrons, but that is convenience, not the rule.
    std::vector<OverdueNotice> notices;
    Status allowed = lib.overdueReport(currentUserId, notices);
    if (allowed != Status::OK) {
        std::cout << "\n   " << statusMessage(allowed) << "\n";
        pause();
        return;
    }

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

    std::vector<std::string> isbns;
    Status allowed = lib.inventoryReport(currentUserId, isbns);
    if (allowed != Status::OK) {
        std::cout << "\n   " << statusMessage(allowed) << "\n";
        pause();
        return;
    }

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
