# Smart Library Management System
## Technical Report and Project Plan

CPSC 315 | Michael Pelitera | Final submission (Module 9)

Architecture, implementation, testing, privacy, and limitations are covered
in sections 1-12. The revised project plan, feedback record, and requirements
coverage are in sections 13-15. The user guide is in `README.md`, and
`MP_CPSC315_SystemDesign.pdf` contains both the final and original designs.

---

## 1. Purpose and scope

A single-user console application in C++17 that manages a book catalog, user
accounts, borrowing and returning with deadline tracking, reservations,
activity and undo, recommendations, authentication, and catalog compression. All state is held in memory for the life of the
process.

Out of scope: persistence to disk, networked or concurrent access, and a
graphical interface. Section 11 covers what each of those would require.

---

## 2. Architecture

Four principal layers separate the interface, coordination, structures,
and records. Password hashing and compression are supporting services.

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
| Title search | Hash index from word to ISBNs | one-word query | O(1) average plus bucket list |
| Password records | PBKDF2-HMAC-SHA256 | verify | deliberately slow: 600,000 iterations |
| Catalog archive | Huffman code over the export | encode, decode | O(n log k), k distinct symbols |
| Reservations | Linked FIFO queue per title, with tail pointer | enqueue, dequeue | dequeue O(1); enqueue O(k) from the duplicate scan; cancel O(k) |

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

### What a co-borrow weight counts

An edge weight is the number of co-borrowing **events**, not the number of
distinct patrons. One patron who borrows the same pair of titles three times
raises the weight by three. For a small library this overstates how widely a
pairing is shared, and it is the reason a minimum-weight threshold would not
by itself guarantee that a suggestion came from more than one person. Counting
distinct borrowers would require retaining who borrowed what, which section 8
explains the system deliberately does not do for the graph.

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
    NOTHING_TO_UNDO, UNDO_NOT_POSSIBLE,
    NOT_AUTHORIZED, ALREADY_EXISTS, STILL_IN_USE
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
make            # both binaries
make test       # build and run the suite
make verify     # tests, sanitizers and demo determinism, in one command
```

The Makefile holds the compiler commands and source lists. `README.md`
explains the build environment; `VERIFICATION.txt` contains the captured
results and a section-by-section reconciliation of the 304 checks.

`./slms_tests` runs 304 checks across twenty-one sections: unit tests per structure,
integration tests over full checkout and return cycles, stress tests at 5,000
books and 2,000 loans, a regression suite for the defects found in Module 6,
interface-state coverage of the demo fixture, published cryptographic test
vectors, and unit plus integration coverage of every structure added in
Module 9.

The supplied verification record reports a warning-free build with
`-Wall -Wextra -Wpedantic`, 304 passing checks, and clean AddressSanitizer
and UndefinedBehaviorSanitizer runs.

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
    AccountLoans, OverdueReport, InventoryTotals,
    RecentActivity, HoldLists
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

Section 11 of the test driver walks `allScreens()`. The screen enum and
that list must be updated together when a new screen is added.

Verified by reintroducing the original defect: cutting the borrowing history
out of `buildDemoLibrary` drops the suite from 304 passing to 298 of 304,
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
and wrong for a running system. Section 11 covers what a production version
would need instead.

---

## 7. Defects found and fixed

| Found in | Defect | Fix |
|---|---|---|
| Module 6 | One loan ID stored per account and title, so a second copy orphaned the first heap entry and produced a phantom overdue notice | Store a list of loan IDs, cleared oldest first |
| Module 6 | Neighbour trimming removed dropped edges from one endpoint of an undirected graph | Remove both halves |
| Module 8 | Two screens rendered empty because the fixture held no borrowing history and lived where tests could not see it | Fixture moved to a shared module, screens enumerated, coverage asserted |
| Module 9 | Signed overflow in the stress-test generator | Arithmetic moved to `uint32_t` |
| Module 9 review | Undoing a return re-dated the loan from today | Activity entries carry the original loan ID and due date; `restoreLoan` replaces the checkout call |
| Module 9 review | Undoing the newer of two loans on one title removed the older | Return path accepts a specific loan ID |
| Module 9 review | A blocked reservation was dropped without fulfilment and the copy sat idle | Offer the copy down the queue; only a successful checkout removes anyone |
| Module 9 review | `loansFor` returned each loan once per copy held | Expand each ISBN once |
| Module 9 review | `make verify` masked failures | Every step propagates its exit status |

Test suite across the project: 60 checks at the start of Module 6, 72 at its
end, 163 after the Module 9 fixture work, 304 in this package.

---

## 8. Recommendation data and privacy

Borrowing records are sensitive. What a person reads is not the same class of
data as what a person owes, and a recommendation feature is the part of this
system most likely to leak it.

### What the current build actually retains

Three facts, all verifiable in the source:

1. `Recommender` never receives a user ID. `recordCoBorrow(isbnA, isbnB)`
   increments a weight on a title-to-title edge. The graph stores *these two
   titles were borrowed together N times* and nothing about who did it.
2. `User::borrowedIsbns` holds only titles **currently** checked out.
   `returnBook` erases the entry.
3. **`ActivityLog` does retain account-linked history.** Each entry records
   the account, the action, the ISBN and the day, including returns, and
   nothing clears it at sign-out. It persists for the life of the process.

The accurate statement is therefore: the recommendation graph stores
title-to-title weights without user IDs, and separately the activity log
retains account-linked borrowing and return events in memory until the
program exits.

What the retained log would need in a real deployment: a retention window
rather than process lifetime, clearing on sign-out or after a fixed period,
staff access to it logged, and patron visibility into what it holds. None of
those exist. The log is what makes undo possible, so removing it is not the
answer; bounding it is.

### Recommendation coverage

The recommendation engine uses current loans as its starting point and does
not use the activity log as a reading-history profile. An account with no
current loans, or with no useful graph connections, can receive no suggestions.
Using retained personal history could extend coverage but would also expand
the information used for profiling and the access and retention controls needed.

### Residual risk in the aggregate graph

De-identified is not the same as anonymous. A low-weight connection can expose a rare co-borrowing event. In a small
collection, outside knowledge may make it possible to associate that event
with an individual.

A minimum weight before an edge becomes usable is the usual mitigation, and it
would raise the bar. It would **not** guarantee that a suggestion came from
more than one borrower, because a weight counts co-borrowing events rather
than distinct patrons: one person who borrows the same pair three times moves
the weight to three on their own. Making the threshold mean what it appears to
mean would require counting distinct borrowers, which means retaining who
borrowed what, which is the thing the graph currently avoids.

Either way it costs coverage: new and niche titles never accumulate enough
weight to be recommended. The current build implements neither; `MAX_NEIGHBORS`
bounds the graph for performance, not privacy.

### What a real deployment would need

| Safeguard | Why | Cost |
|---|---|---|
| Minimum edge weight, counted over distinct borrowers | Raises the bar on tracing a suggestion to one person; counting events instead would not | Niche titles stop surfacing, and distinct counting means retaining who borrowed what |
| Opt-in rather than opt-out | Reading records are not ordinary telemetry | Smaller graph, weaker suggestions |
| Recommendations hidden on shared or staff screens | The patron is not always the only person looking | More interface states to build and test |
| Documented retention window on the aggregate graph | Edges currently last until process exit | Older co-borrow signal is lost |
| Staff access to a patron's current loans logged | Legitimate need, but it should leave a trace | An audit log, which the system does not have |

---

## 9. Security

### Authentication

Sign-in verifies an account ID and password. Password verification was
added in Module 9; earlier versions accepted the ID alone.

Password records use PBKDF2-HMAC-SHA256 with a randomly generated 16-byte
salt and a default work factor of 600,000 iterations. Seeded demonstration
accounts use a reduced factor of 10,000 so the demo and test suite stay
usable; accounts created through the staff interface use the default. The
iteration count is stored in each record, so both verify correctly and
raising the default later does not lock out existing accounts.

OWASP recommends Argon2id first and scrypt where Argon2id is unavailable; its
guidance specifies PBKDF2-HMAC-SHA256 at a minimum of 600,000 iterations
where FIPS-140 compliance is required. This project uses PBKDF2 as an
educational implementation choice, because it has no external dependencies
and the chain from SHA-256 through HMAC to PBKDF2 is implementable and
checkable in a course project. That is the rationale; it is not an exception
OWASP grants.

Plaintext passwords are handled transiently during entry and verification.
Account records store only the salted hash record.

The test suite includes published known-answer vectors for SHA-256,
HMAC-SHA256 and PBKDF2-HMAC-SHA256. Those confirm the primitives compute what
the standards define. They are not a security audit.

### Authorization

Role checks run in `LibrarySystem`, not in the interface. The console passes
the authenticated account ID to the reporting and staff-management methods,
which enforce the check there and return `NOT_AUTHORIZED` on refusal, clearing
any output buffer the caller passed in.

Scope, stated plainly: the low-level structure accessors (`books()`,
`accounts()` and the rest) remain public for fixtures and internal use, so the
project does not claim that every public method enforces authorization. That
is a limitation of a teaching implementation. `overdueNotices()` was public
and unguarded until the second review round and is now private behind
`overdueReport()`.

---

## 10. Compression

`catalogExport()` serializes the catalog to one line per record. `Huffman`
codes it: frequencies counted, a min-heap merged pairwise into a tree, codes
read off root-to-leaf paths, output bit-packed with an explicit bit count so
decoding does not walk into the padding.

The result at demo scale is worth stating because it looks like a failure.
Six titles produce a 355-byte export, which codes to a 231-byte payload and a
255-byte frequency table: 486 bytes stored, a ratio of 1.37. The archive is
larger than the input. The table is a fixed cost of five bytes per distinct
symbol and six books do not produce enough text to amortize it.

At 5,000 titles the same code achieves a ratio of 0.61 on a 397,624-byte
export. Both numbers are printed by the staff storage report and asserted in
test section 17, because a compression feature that only shows its good case
is not being honest about the trade.

---

## 11. Known limitations

| Limitation | Consequence | What it would take |
|---|---|---|
| No persistence | All state is lost when the process exits | Serialize on exit and reload at startup, or append an event log and replay it |
| Single-threaded | Concurrent calls are unsupported and could race over copies and loans | Coordinate locking across the full operation, including inventory, account indexes, deadlines, and reservations |
| Validity checks are O(n) sweeps | Too expensive to run continuously in production | Sampled or incremental checks; verify the affected subtree after a mutation rather than the whole structure |
| Recommendation tree rebuilt per request | Wasted work when a user reloads the same list | Cache per user and invalidate on checkout |
| Password policy is one password per account | No lockout, no reset path, no session timeout, no rate limiting on attempts | Attempt throttling, an account recovery route, and a session that expires |
| Dates are integer day numbers | No clock, no calendar arithmetic | Real date type; the comparison logic does not change |

### Specified but not implemented

The following commitments from the original Module 1 plan and Module 2
design remain unimplemented. Required feature coverage is listed in section 15.

**Built in Module 9, previously listed here:** the activity log stack, the
reservation queue, Huffman coding, password hashing, the secondary title
index, and sorting by publication date. All six are implemented, covered by
the test driver, and reachable from the interface.

| Feature | Where it was specified | Why it is absent / what it would take |
|---|---|---|
| Persistence to flat files | Module 1 scope: "data persists to flat files loaded into memory at startup" | Serialize on exit and reload at startup, or append an event log and replay it. Every structure would need a load path that rebuilds its index |
| Append-only event log | Module 1 plan: one log as the single source of truth, each component subscribing | Not built. `LibrarySystem` coordinates the modules directly instead. The `ActivityLog` is a per-account stack whose entries undo can remove, which is a different thing from an append-only log |
| Repository interfaces over the structures | Module 1 plan: swap a hash table for a B-tree without touching callers | Not built. `LibrarySystem` holds concrete members and exposes them directly |
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

## 12. File reference

| File | Contents |
|---|---|
| `src/Book.h` | Book record, one per ISBN |
| `src/User.h` | Account record and Role enum |
| `src/BookInventory.h/.cpp` | Hash table with chaining and resizing |
| `src/UserAccounts.h/.cpp` | Sorted list plus index, with `validate()` |
| `src/DueDateHeap.h/.cpp` | Indexed min-heap on due date |
| `src/Recommender.h/.cpp` | Co-borrowing graph and score BST |
| `src/ActivityLog.h/.cpp` | Per-account stack of actions, with validity check |
| `src/Sha256.h/.cpp` | SHA-256, HMAC-SHA256, PBKDF2-HMAC-SHA256 |
| `src/PasswordHash.h/.cpp` | Salted password records built on PBKDF2 |
| `src/Huffman.h/.cpp` | Huffman tree, encoder, decoder |
| `src/ReservationQueue.h/.cpp` | Per-title FIFO hold line, with validity check |
| `src/LibrarySystem.h/.cpp` | Coordination layer, Status enum |
| `src/LibraryUI.h/.cpp` | Console interface |
| `src/DemoFixture.h/.cpp` | Seed data, screen enumeration, coverage check |
| `src/app.cpp` | Interactive entry point |
| `src/main.cpp` | Test driver, 304 checks |

## 13. Project plan

### Scope and deliverables

The final scope is the C++17 console system described in section 1. Deliverables
are the application and tests, this technical report and revised plan, the user
guide in `README.md`, the system design PDF, the PowerPoint presentation, and
the demonstration and verification records. Persistence, networking,
concurrent access, a GUI, and payment processing are outside the final scope.
Persistence was an original commitment that was not delivered; compression
therefore operates on the in-memory catalog export instead of an archived
transaction log.

### Timeline and milestones

| Module | Planned | Delivered |
|---|---|---|
| 1 | Requirements, scope, plan | Plan written |
| 2 | UML, structure selection, mock-ups | Design document with architecture diagram, class diagram, two mock-ups |
| 3 | Inventory: ISBN hash table, **secondary title index**, sorting | Hash table and sorting delivered. The title index slipped to Module 9 |
| 4 | Accounts: linked list, **activity stack**, **reservation queue** | Linked list and index delivered. Stack and queue slipped to Module 9 |
| 5 | Borrow/return, indexed min-heap, BST and graph recommender | Delivered as planned |
| 6 | Unit, integration and load tests | Delivered. Two defects found and fixed; suite 60 → 72 checks |
| 7 | User manual, technical report, interface | Delivered, plus four interface defects found by writing the manual |
| 8 | Presentation and scripted demo | Delivered. Dry run exposed two empty screens |
| 9 | Final submission incorporating feedback | This package. Four requirement gaps closed, seven defects fixed, suite 72 → 304 checks |

The title index moved from Module 3 to Module 9, six modules later. The
activity stack and reservation queue moved from Module 4 to Module 9, five
modules later. All three are structures the specification names, and all three
were caught by an audit against the requirements list rather than by the weekly
process, which is the process failure worth recording.

### Resources

| Resource | Planned and actual use |
|---|---|
| Language and compiler | C++17 and g++; final flags include `-O2 -Wall -Wextra -Wpedantic` |
| Testing | A small C++ check helper replaced Catch2; ASan and UBSan are run by `make verify` |
| Project history | Per-module archives were used instead of the planned Git branches and tags |
| Test data | Six demonstration titles and generated stress data: 5,000 books and 2,000 loans |
| Design and presentation | Module 2 diagrams and mock-ups, a final design revision, and an eight-slide PowerPoint |
| Time | The original estimate was 8-10 hours per week, with more work in Modules 6 and 9 |

Tooling differences from the original plan, including Doxygen, PlantUML source
files, and Open Library data, are recorded in section 11.

### Risks and acceptance criteria

| Risk or criterion | Outcome |
|---|---|
| Early returns leave invalid heap entries | Loan IDs and an index map support removal; duplicate-title cases have regression coverage |
| Poor hash distribution on similar ISBNs | FNV-1a and resizing tested with 5,000 shared-prefix ISBNs; longest chain 5 in the captured run |
| Title searching differs from exact ISBN lookup | Word index implemented in Module 9; partial words can fall back to a scan |
| Required features slip while individual modules appear complete | Title index arrived six modules late; activity stack and reservation queue five modules late |
| Correct overdue order and stable operations | Unit, integration, stress, and regression checks are included in the final suite |
| 100,000 records loaded in under two seconds | Not measured; the largest inventory test uses 5,000 records |
| Recommendations for anyone with three loans | Revised: suggestions require useful co-borrowing connections, not a particular loan count |
| User and developer documentation | User guide in `README.md`; technical reference and requirements coverage here |

For a second attempt, review requirements at each milestone, give stored facts
clear owners and check their indexes, test the interface's data states, keep
verification failures visible through exit codes, and use version control.

## 14. Feedback and changes

The table connects course feedback and final review findings to implementation
decisions. Defect mechanisms and fixes are summarized in section 7; remaining
limitations are in sections 8, 9, and 11.

| Module | Feedback received | Decision made | Evidence in this package | Remaining limitation |
|---|---|---|---|---|
| 1 | Instructor asked which clustering or dimensionality-reduction techniques would keep the recommender tractable at scale | Bound the neighbour list per title rather than cluster users | `Recommender::MAX_NEIGHBORS = 50`, `trimNeighbors`, tests in section 8 | MinHash and LSH were discussed and not built; the score tree is rebuilt per request, though the graph itself persists |
| 2 | Instructor asked how a hold counter would stay accurate under concurrent reservations | Do not keep a separate counter at all; let the queue own its length | `ReservationQueue::lengthOf` is O(1) from `Line::count`; the header records why `Book::activeHolds` was dropped | The system is single-threaded, so concurrency protection remains future work |
| 3 | Instructor and a peer raised AVL against Red-Black for frequently updated scores | Neither. Scores are never updated in place: the tree is cleared and rebuilt per request from a bounded candidate set | `Recommender::recommend` calls `clearTree()` then inserts; header states the tree is unbalanced and why that is acceptable here | An unbalanced BST degrades on sorted input. Bounded candidate sets keep it shallow in practice, but nothing enforces that |
| 4 | Instructor asked about thread safety across the account list and its index | Documented the intended lock covering both together, rather than implementing one | Technical report section 9 | Not implemented |
| 6 | Instructor asked how invariant checkers could validate graph relationships after every mutation | Added per-structure `validate()` and `validateAll()`, called at checkpoints in the test driver | `UserAccounts::validate`, `DueDateHeap::validate`, `Recommender::validateTree`, `ActivityLog::validate`, `ReservationQueue::validate` | Partly addressed. `validateAll` does not check graph edge symmetry, nor cross-structure agreement between loans, accounts and inventory. Both gaps are covered only by targeted regression tests |
| 6 | Peer suggested deliberately colliding keys rather than hoping for collisions | Stress test uses 5,000 ISBNs sharing a prefix and asserts the longest chain | Section 2, measured: 8,192 buckets, load factor 0.61, longest chain 5 | No test uses known-colliding ISBNs to confirm other records in a chain survive an update or delete |
| 7 | Peer questioned hiding restricted options against showing and explaining them | Kept hiding, because an option that always refuses is noise | `LibraryUI::mainMenu` prints staff options only for staff | The user guide now explains role differences so a patron understands why their menu is shorter |
| 8 | Instructor asked how test fixtures could verify that mock data covers all dynamic interface states automatically | Enumerate the screens, declare what state each fixture targets, assert coverage | `DemoFixture.h` `Screen` enum, `screenCoverage`, three fixtures, test section 11 | The enum and `allScreens()` list must stay synchronized |
| 9 | External review: the permission claim in the Module 7 post was false | Move the checks into the logic layer rather than withdraw the claim | `LibrarySystem::isStaff`, `overdueReport`, `inventoryReport`, `holdReport`, the four management operations; section 15 | The low-level structure accessors remain public for fixtures, so not every public method enforces authorization |
| 9 | External review: iterated FNV-1a is not a password hash | Replaced with PBKDF2-HMAC-SHA256 over a from-scratch SHA-256 and HMAC | `Sha256.*`, `PasswordHash.*`, published vectors in section 16 | Argon2id is the preferred algorithm. PBKDF2 was chosen because the project has no external dependencies |
| 9 | External review: three undo and reservation defects | Fixed each, with a regression test naming the sequence | Section 15 | Undoing a cancelled reservation rejoins at the back of the queue; the current implementation does not restore the original position |
| 9 | External review: `make verify` could print failures and still succeed | Every failing step now returns a non-zero exit status | `Makefile` verify target, checked by deliberately breaking a test and the recorded demo | None |
| 9 | External review: duplicate-title loans listed four times with one due date | Expand each ISBN once; show one row per loan | `LibrarySystem::loansFor`, `LibraryUI::myAccount`, section 21 | None |

## 15. Requirements coverage

| Requirement | Status | Evidence |
|---|---|---|
| Hash tables for book lookup by ISBN | Built | `BookInventory`, FNV-1a with chaining, sections 1 and 2 |
| ...or by title | Built (Module 9) | `BookInventory` word index; section 19. Prefix queries still fall back to a substring scan |
| Sorting by title | Built | `isbnsSortedByTitle`, section 9 |
| Sorting by author | Built | `isbnsSortedByAuthor`, section 9 |
| Sorting by publication date | Built (Module 9) | `Book::year`, `isbnsSortedByYear`, section 18 |
| Linked lists for user accounts | Built | `UserAccounts`, sorted doubly linked list plus index, section 3 |
| Stacks for user activity logs | Built (Module 9) | `ActivityLog`, section 12, undo integration in section 14 |
| Queues for reservation processing | Built (Module 9) | `ReservationQueue`, section 13, fulfilment in section 14 |
| Efficient searching for books and users | Built | O(1) average on both, sections 1 and 3 |
| Heaps and priority queues for deadlines | Built | `DueDateHeap`, indexed min-heap, sections 5 and 6 |
| Trees for recommendations | Built | Score BST with reverse in-order ranking, section 7 |
| Graphs for recommendations | Built | Co-borrow adjacency list, sections 7 and 8 |
| Huffman coding for storage optimization | Built (Module 9) | `Huffman`, over `catalogExport()`; section 17 |
| Hashing for password security | Built (Module 9) | PBKDF2-HMAC-SHA256 wired into sign-in; section 16 |
| User-friendly interface | Built | `LibraryUI`, role-gated menus, named refusals |
| Documentation for developers and end users | Built | `TECHNICAL_REPORT.md` and the user guide in `README.md` |

All named feature groups are represented. Original commitments that changed
or remain unimplemented are documented in sections 11 and 13.
