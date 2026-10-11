# Smart Library Management System
## Technical Report

CPSC 315 | Michael Pelitera | Final submission (Module 9)

Originally written for Module 7. Sections 6 and 7 were extended in Module 9
with the test-fixture work and the two defects found during final review.

---

## 1. Purpose and scope

A single-user console application in C++17 that manages a book catalog, user
accounts, borrowing and returning with deadline tracking, and a
recommendation engine. All state is held in memory for the life of the
process.

Out of scope: persistence to disk, networked or concurrent access, and a
graphical interface. Section 9 covers what each of those would require.

---

## 2. Architecture

Four layers. Each depends only on the layer beneath it.

```
  Presentation     LibraryUI              console menus, input validation
  ---------------------------------------------------------------------
  Coordination     LibrarySystem          checkout, return, reports
  ---------------------------------------------------------------------
  Structures       BookInventory          hash table on ISBN
                   UserAccounts           sorted list + index
                   DueDateHeap            indexed min-heap
                   Recommender            graph + score BST
                   ActivityLog            stack per account
                   ReservationQueue       FIFO queue per title
  ---------------------------------------------------------------------
  Records          Book, User, Loan       plain data
```

The rule that matters: **no structure module calls another structure module.**
`BookInventory` does not know `UserAccounts` exists. Every operation touching
more than one structure goes through `LibrarySystem`.

This is why the test driver can exercise the logic without simulating
keystrokes, and why `LibraryUI` could be replaced with a graphical front end
without changing a line below it.

---

## 3. Data structures and their costs

| Component | Structure | Operations | Cost |
|---|---|---|---|
| Book inventory | Hash table, separate chaining, FNV-1a | insert, find, erase | O(1) average |
| User accounts | Sorted doubly linked list + hash index | insert, find, erase | O(1) find and erase; O(1) append for increasing IDs |
| Due dates | Binary min-heap + loan ID to index map | push, pop-min, remove | O(log n) all three |
| Recommendations | Adjacency list graph + score BST | walk, rank | bounded by 50 neighbors per title |
| Sorted views | On-demand stable sort of ISBN vector | sort | O(n log n) per request |
| Activity log | Linked stack per account | push, pop, peek | O(1) all three |
| Reservations | Linked FIFO queue per title, with tail pointer | enqueue, dequeue | O(1); cancel is O(k) in line length |

### Why FNV-1a rather than a character sum

ISBN-13 values are highly structured. Every one begins 978 or 979, followed by
registration group and publisher digits, so books from the same publisher
share a long prefix. A hash that accumulates character values maps those into
a narrow band of buckets.

FNV-1a XORs each byte into the running hash and multiplies by a prime, so a
shared prefix does not concentrate results. Verified in test section 2: 5,000
ISBNs sharing a `9780262` prefix, 8,192 buckets, load factor 0.61, longest
chain 5.

### Why the account list carries a hash index

The project specification requires a linked list for accounts. A list alone
makes lookup O(n). The index maps user ID to the node holding that account,
which gives O(1) average lookup and makes deletion O(1), since the node is
handed over directly and a doubly linked list only needs its two neighbors
relinked.

Cost: two structures that can disagree. `UserAccounts::validate()` checks list
ordering, forward and backward links, the count, and that every index entry
points at the node actually holding that account.

### Why the heap carries a position map

A plain binary heap supports push and pop-min in O(log n) but cannot locate an
arbitrary element without an O(n) scan. Early returns are the most frequent
write the system handles, so that scan would run constantly.

A hash map from loan ID to array index, updated inside every sift, makes
removal O(1) to locate plus O(log n) to restore the heap property.

---

## 4. Key workflows

### Checkout

```
LibraryUI::searchCatalog or borrowFlow
  -> LibrarySystem::checkout(userId, isbn, &loanId)
       validate: account exists            O(1) avg
       validate: under 5-loan limit        O(1)
       validate: book exists               O(1) avg
       validate: a copy is available       O(1)
       --- no writes before this line ---
       update co-borrow graph edges        O(k), k = current loans
       decrement copiesAvailable           O(1)
       append ISBN to account              O(1)
       push loan onto due-date heap        O(log n)
       record loan ID for the pair         O(1)
```

Every check runs before the first write. A rejected checkout cannot leave the
copy count decremented with no loan recorded.

### Return

Confirms the loan is on that account before restoring the copy, then removes
the heap entry **by loan ID**, which is what makes an early return O(log n)
rather than O(n).

---

## 5. Error handling

Operations return a `Status` enum, not a boolean:

```cpp
enum class Status {
    OK, USER_NOT_FOUND, BOOK_NOT_FOUND,
    NO_COPIES_AVAILABLE, LOAN_LIMIT_REACHED, NOT_CHECKED_OUT,
    ALREADY_RESERVED, NOT_RESERVED, COPY_IS_AVAILABLE,
    NOTHING_TO_UNDO, UNDO_NOT_POSSIBLE
};
```

`LOAN_LIMIT_REACHED` and `NO_COPIES_AVAILABLE` are different problems for the
patron, and the interface can only say which applies if the function
distinguishes them. `statusMessage()` maps each value to the text shown on
screen, so message wording lives in one place rather than scattered through
the UI.

---

## 6. Verification

Two executables build from the same source tree:

```
g++ -std=c++17 -Wall -Wextra -o slms_app \
    src/app.cpp src/LibraryUI.cpp src/BookInventory.cpp \
    src/UserAccounts.cpp src/DueDateHeap.cpp \
    src/Recommender.cpp src/LibrarySystem.cpp src/DemoFixture.cpp

g++ -std=c++17 -Wall -Wextra -o slms_tests \
    src/main.cpp src/BookInventory.cpp src/UserAccounts.cpp \
    src/DueDateHeap.cpp src/Recommender.cpp src/LibrarySystem.cpp \
    src/DemoFixture.cpp
```

`./slms_tests` runs 163 checks across fourteen sections: unit tests per structure,
integration tests over full checkout and return cycles, stress tests at 5,000
books and 2,000 loans, a regression suite for the defects found in Module 6,
interface-state coverage of the demo fixture, and unit plus integration
coverage of the activity stack and reservation queue.

Status: compiles clean under `-Wall -Wextra -Wpedantic`; AddressSanitizer and
UndefinedBehaviorSanitizer report no leaks or undefined behavior.

### Interface-state coverage

The test suite can confirm that an operation succeeds. It cannot, by itself,
confirm that the screen built on that operation has anything to display. Those
are different properties, and the second one failed in Module 8 while the
first passed: the recommendation and overdue screens rendered empty because
the seed data held a catalog and accounts but no borrowing history.

The gap was structural. Seed data lived inside `LibraryUI`, and `slms_tests`
does not link the interface layer, so no test could see it.

`src/DemoFixture.h` closes this. It enumerates the dynamic screens, meaning
the ones whose content depends on system state rather than on layout:

```cpp
enum class Screen {
    SearchResults, UnavailableTitle, Recommendations,
    AccountLoans,  OverdueReport,    InventoryTotals
};
```

Three fixtures each declare the state they exist to produce.
`buildDemoLibrary` targets the populated state of every screen,
`buildEmptyHistoryLibrary` reproduces the Module 8 condition so the empty
rendering stays covered on purpose, and `buildEmptyLibrary` checks that an
empty catalog does not fault the screens that read it.

`screenCoverage()` replays the scripted demo path and reports a row count per
screen. The replay is necessary: the account and recommendation screens are
legitimately empty for the demo patron until the scripted checkout runs, so
inspecting the fixture at rest would report a failure that is not real. The
check measures what the audience sees.

Section 11 of the test driver walks `allScreens()` rather than a hand-written
list, so adding a screen without extending the check is not possible.

Verified by reintroducing the original defect: cutting the borrowing history
out of `buildDemoLibrary` drops the suite from 163 passing to 157 of 163,
naming the unavailable-title, recommendation, overdue, and hold-list screens.
Before this section existed, the same change passed every test.

### Invariant checks

Each structure owns a validity check, following the BST validity checker
pattern from Lab 7.15:

- `UserAccounts::validate()` — ordering, links, count, index agreement
- `DueDateHeap::validate()` — heap property and index map agreement
- `Recommender::validateTree()` — BST keys against inherited ancestor bounds
- `ActivityLog::validate()` — chain lengths against recorded depths
- `ReservationQueue::validate()` — line lengths, tail pointers, no duplicates
- `LibrarySystem::validateAll()` — runs all five

These are O(n) full sweeps, appropriate after each operation in a test driver
and wrong for a running system. Section 9 covers what a production version
would need instead.

---

## 7. Defects found and fixed

### Module 6 testing

**Orphaned heap entry on a duplicate-title loan.** The loan lookup stored one
loan ID per (user, ISBN) pair, assuming a user could never hold two copies of
one title. Nothing enforced that. The second checkout overwrote the first loan
ID, leaving the first heap entry unreachable and generating a phantom overdue
notice for a returned book. Fixed by storing a list of outstanding loan IDs
per pair, cleared oldest-first.

**Asymmetric edges after neighbor trimming.** The 50-neighbor cap removed
dropped edges from one endpoint only. The co-borrowing graph is undirected, so
every trimmed neighbor kept a return edge to a vertex that had forgotten it.
Fixed by removing both halves.

Both passed the then-existing 60-test suite. Both now have regression tests.

### Module 8 rehearsal

**Empty recommendation and overdue screens.** Covered in section 6. Not a
code defect: every operation behind those screens worked. The seed data did
not reach the state the screens were built to show. Fixed by moving the
fixture into a module the tests link and asserting screen coverage against it.

### Module 9 review

**Signed integer overflow in the stress-test generator.** The linear
congruential generator seeding section 6's 2,000 loans multiplied in `int`.
It overflows on its first step by design, and signed overflow is undefined
behavior, so the test was relying on something the standard does not define.
UndefinedBehaviorSanitizer reported it at `main.cpp:233` once the sanitizer
run was repeated for the final submission. Fixed by doing the arithmetic in
`uint32_t`, where wrapping is defined and is what the generator wants.

This one was in test code rather than the library, so it could not affect a
user. It is recorded because the Module 7 report claimed the build was clean
under UndefinedBehaviorSanitizer, and that claim was wrong at the time it was
written. The sanitizer run is now part of the build verification rather than
something done once.

---

## 8. Recommendation data and privacy

Borrowing records are sensitive. What a person reads is not the same class of
data as what a person owes, and a recommendation feature is the part of this
system most likely to leak it.

### What the current build actually retains

Two facts, both verifiable in the source:

1. `Recommender` never receives a user ID. `recordCoBorrow(isbnA, isbnB)`
   increments a weight on a title-to-title edge. The graph stores *these two
   titles were borrowed together N times* and nothing about who did it.
2. `User::borrowedIsbns` holds only titles **currently** checked out.
   `returnBook` erases the entry. There is no per-patron reading history in
   the system after a book goes back.

So the only durable trace of what someone read is an anonymous increment on a
shared counter, and `recommendationsFor` works from the shelf the patron is
holding right now rather than from a profile.

This was not a privacy decision. It fell out of the data model, and it is
worth recording as a property the design happens to have rather than as
foresight. A later change that adds a `readingHistory` vector to `User` for
better recommendations would silently remove it.

### The trade-off this buys and costs

Forgetting is cheap to implement and strong as a safeguard: data not retained
cannot be disclosed, subpoenaed, or breached. The cost is recommendation
quality. A patron with nothing checked out gets no recommendations, because
there is no history to walk from. Every empty shelf is a cold start.

The alternative, retaining per-user history, improves suggestions and creates
a reading record that has to be protected, retained on a schedule, and handed
over if someone with authority asks. That is the actual architectural choice,
and it is a policy question wearing a data-structure costume.

### Residual risk in the aggregate graph

De-identified is not the same as anonymous. An edge with weight 1 encodes that
exactly one patron borrowed both of those titles. In a small collection, with
some outside knowledge of who was in the building, that is re-identifying.

The standard mitigation is a minimum weight before an edge becomes usable, so
no suggestion can be traced to a single borrower. It costs coverage: new and
niche titles never accumulate enough weight to be recommended, which is the
same cold-start problem in a different place. The current build does not
implement this; `MAX_NEIGHBORS` bounds the graph for performance, not privacy.

### What a real deployment would need

| Safeguard | Why | Cost |
|---|---|---|
| Minimum edge weight before use | No suggestion traceable to one borrower | Niche titles stop surfacing |
| Opt-in rather than opt-out | Reading records are not ordinary telemetry | Smaller graph, weaker suggestions |
| Recommendations hidden on shared or staff screens | The patron is not always the only person looking | More interface states to build and test |
| Documented retention window on the aggregate graph | Edges currently live forever | Older co-borrow signal is lost |
| Staff access to a patron's current loans logged | Legitimate need, but it should leave a trace | An audit log, which the system does not have |

---

## 9. Known limitations

| Limitation | Consequence | What it would take |
|---|---|---|
| No persistence | All state is lost when the process exits | Serialize on exit and reload at startup, or append an event log and replay it |
| Single-threaded | Two simultaneous checkouts of the last copy would both succeed | One mutex covering the account list and its index together; the invariant spans both, so locking separately does not help |
| Validity checks are O(n) sweeps | Too expensive to run continuously in production | Sampled or incremental checks; verify the affected subtree after a mutation rather than the whole structure |
| Recommendation tree rebuilt per request | Wasted work when a user reloads the same list | Cache per user and invalidate on checkout |
| Passwords stored as a hash field only | No hashing implementation is wired in | An established password-hashing function; the field and the interface already exist |
| Dates are integer day numbers | No clock, no calendar arithmetic | Real date type; the comparison logic does not change |

### Specified but not implemented

These were committed to in the Module 1 and 2 design documents, or appear in
the project's key features list, and are not present in the current build.
They are recorded here so the gap is visible to a future developer rather
than discovered by searching the source.

This list was audited against the Module 1 project plan, the Module 2 design
document, and the project's own key features list before final submission.
Several entries below were missing from earlier versions of this table, which
made the gap record itself incomplete.

**Built in Module 9, previously listed here:** the activity log stack and the
reservation queue. Both are now implemented, covered by sections 12 to 14 of
the test driver, and reachable from the interface.

| Feature | Where it was specified | Why it is absent / what it would take |
|---|---|---|
| Huffman coding | Key features: compression for storage optimization | Only meaningful once persistence exists, since there is nothing on disk to compress. Would apply to an archived transaction log rather than individual records |
| Password hashing | Key features: hashing for password security | `User::passwordHash` exists and the sign-in path reads it, but no hashing function is wired in. Needs an established password-hashing algorithm, not one written here |
| Persistence to flat files | Module 1 scope: "data persists to flat files loaded into memory at startup" | Serialize on exit and reload at startup, or append an event log and replay it. Every structure would need a load path that rebuilds its index |
| Secondary title index | Module 1 plan, Module 3 milestone: "ISBN hash table, secondary title index" | Title search is currently a linear scan of the catalog. An inverted index from tokenized titles and authors to ISBNs would make it sub-linear. The plan called for this in Module 3 and it was never built |
| Sorting by publication date | Key features: display by title, author, or publication date | `Book` has no date field at all. Adding one is trivial; the sort is the same stable sort already used for title and author |
| Fines | Module 2 borrowing rules: no checkout with unpaid fines over a threshold | A balance field on `User`, a rate, and a fourth validation step in `checkout`. `Status` would need a new value so the interface can name this rejection |
| Renewals | Module 2 borrowing rules: no renewal on a title with a waiting hold | The hold half now exists, so the check is available. A renewal is a due-date change, so the heap entry would be removed and re-pushed rather than edited in place |

### Tooling claimed in Module 1 and not used

Recorded for the same reason: the plan is part of the project record, and a
reader comparing it against the repository would otherwise find the
difference unexplained.

| Claimed | What was actually done |
|---|---|
| Catch2 for unit tests | A hand-written `check()` helper in `main.cpp`. No external test dependency |
| Git with one branch per module, tagged at each deliverable | Deliverables were packaged per module as archives instead |
| Doxygen comments in headers | Plain block comments. Readable, but not machine-extractable |
| PlantUML diagrams kept as text in the repo | Diagrams were produced for the Module 2 deck but the sources are not in this package |
| Open Library bulk export for catalog data | A hand-written six-title catalog |
| Load test at 100,000 records, under two seconds | Stress tests run at 5,000 books and 2,000 loans. The 100,000-record figure in the Module 1 definition of done was never measured |

The interface reflects the remaining feature gaps rather than hiding them.
The Module 2 mock-ups showed `[E] renew` and a fines line on the account
screen. Those options are absent from the current menus because selecting
them would do nothing. `[H] place hold` is now present and works.

---

## 10. File reference

| File | Contents |
|---|---|
| `src/Book.h` | Book record, one per ISBN |
| `src/User.h` | Account record and Role enum |
| `src/BookInventory.h/.cpp` | Hash table with chaining and resizing |
| `src/UserAccounts.h/.cpp` | Sorted list plus index, with `validate()` |
| `src/DueDateHeap.h/.cpp` | Indexed min-heap on due date |
| `src/Recommender.h/.cpp` | Co-borrowing graph and score BST |
| `src/ActivityLog.h/.cpp` | Per-account stack of actions, with validity check |
| `src/ReservationQueue.h/.cpp` | Per-title FIFO hold line, with validity check |
| `src/LibrarySystem.h/.cpp` | Coordination layer, Status enum |
| `src/LibraryUI.h/.cpp` | Console interface |
| `src/DemoFixture.h/.cpp` | Seed data, screen enumeration, coverage check |
| `src/app.cpp` | Interactive entry point |
| `src/main.cpp` | Test driver, 163 checks |
