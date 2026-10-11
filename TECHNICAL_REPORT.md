# Smart Library Management System
## Technical Report

CPSC 315 | Michael Pelitera | Module 7 Deliverable

---

## 1. Purpose and scope

A single-user console application in C++17 that manages a book catalog, user
accounts, borrowing and returning with deadline tracking, and a
recommendation engine. All state is held in memory for the life of the
process.

Out of scope: persistence to disk, networked or concurrent access, and a
graphical interface. Section 8 covers what each of those would require.

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
    NO_COPIES_AVAILABLE, LOAN_LIMIT_REACHED, NOT_CHECKED_OUT
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
    src/Recommender.cpp src/LibrarySystem.cpp

g++ -std=c++17 -Wall -Wextra -o slms_tests \
    src/main.cpp src/BookInventory.cpp src/UserAccounts.cpp \
    src/DueDateHeap.cpp src/Recommender.cpp src/LibrarySystem.cpp
```

`./slms_tests` runs 72 checks across ten sections: unit tests per structure,
integration tests over full checkout and return cycles, stress tests at 5,000
books and 2,000 loans, and a regression suite for the defects found in
Module 6.

Status: compiles clean under `-Wall -Wextra -Wpedantic`; AddressSanitizer and
UndefinedBehaviorSanitizer report no leaks or undefined behavior.

### Invariant checks

Each structure owns a validity check, following the BST validity checker
pattern from Lab 7.15:

- `UserAccounts::validate()` — ordering, links, count, index agreement
- `DueDateHeap::validate()` — heap property and index map agreement
- `Recommender::validateTree()` — BST keys against inherited ancestor bounds
- `LibrarySystem::validateAll()` — runs all three

These are O(n) full sweeps, appropriate after each operation in a test driver
and wrong for a running system. Section 8 covers what a production version
would need instead.

---

## 7. Defects found and fixed (Module 6)

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

---

## 8. Known limitations

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

| Feature | Where it was specified | What it would take |
|---|---|---|
| Holds and reservations | Module 2 design: per-title `activeHolds` counter on `Book`, reservation queue per title | A `Hold` record, a FIFO queue keyed by ISBN, and the counter. The Module 2 reasoning stands: keeping a per-title count makes the renewal check O(1) instead of scanning the queue at O(Q) |
| Fines | Module 2 borrowing rules: no checkout with unpaid fines over a threshold | A balance field on `User`, a rate, and a fourth validation step in `checkout`. `Status` would need a new value so the interface can name this rejection |
| Renewals | Module 2 borrowing rules: no renewal on a title with a waiting hold | Depends on holds. A renewal is a due-date change, so the heap entry would be removed and re-pushed rather than edited in place |
| Activity log stack | Project key features: stacks to track user activity | A stack per account, pushed on every state change. The UI has an obvious use for it: an undo for a mis-scan during a multi-item check-in |
| Reservation queue | Project key features: queues for book reservation processing | Same structure as holds above; the two are one feature |
| Huffman coding | Project key features: compression for storage optimization | Only meaningful once persistence exists, since there is nothing on disk to compress. Would apply to an archived transaction log rather than individual records |

The interface reflects these gaps rather than hiding them. The Module 2
mock-ups showed `[H] place hold`, `[E] renew`, and a fines line on the
account screen. Those options are absent from the current menus because
selecting them would do nothing.

---

## 9. File reference

| File | Contents |
|---|---|
| `src/Book.h` | Book record, one per ISBN |
| `src/User.h` | Account record and Role enum |
| `src/BookInventory.h/.cpp` | Hash table with chaining and resizing |
| `src/UserAccounts.h/.cpp` | Sorted list plus index, with `validate()` |
| `src/DueDateHeap.h/.cpp` | Indexed min-heap on due date |
| `src/Recommender.h/.cpp` | Co-borrowing graph and score BST |
| `src/LibrarySystem.h/.cpp` | Coordination layer, Status enum |
| `src/LibraryUI.h/.cpp` | Console interface |
| `src/app.cpp` | Interactive entry point |
| `src/main.cpp` | Test driver, 72 checks |
