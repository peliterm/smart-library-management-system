# Requirements audit

CPSC 315 | Michael Pelitera | Module 9

Every requirement in the project's key features list, and every commitment
made in the Module 1 plan and Module 2 design, checked against the code in
this package. Run before final submission on the assumption that the claims
made across eight modules had drifted from what was actually built. They had.

Legend: **Built** means present and covered by tests. **Partial** means
something is there but does not meet the stated requirement. **Absent**
means not implemented.

---

## Project key features

| Requirement | Status | Evidence |
|---|---|---|
| Hash tables for book lookup by ISBN | Built | `BookInventory`, FNV-1a with chaining, sections 1 and 2 |
| ...or by title | Partial | Title search is a linear scan of the catalog. No index. The Module 1 plan called for one in Module 3 |
| Sorting by title | Built | `isbnsSortedByTitle`, section 9 |
| Sorting by author | Built | `isbnsSortedByAuthor`, section 9 |
| Sorting by publication date | Absent | `Book` has no date field |
| Linked lists for user accounts | Built | `UserAccounts`, sorted doubly linked list plus index, section 3 |
| Stacks for user activity logs | Built (Module 9) | `ActivityLog`, section 12, undo integration in section 14 |
| Queues for reservation processing | Built (Module 9) | `ReservationQueue`, section 13, fulfilment in section 14 |
| Efficient searching for books and users | Built | O(1) average on both, sections 1 and 3 |
| Heaps and priority queues for deadlines | Built | `DueDateHeap`, indexed min-heap, sections 5 and 6 |
| Trees for recommendations | Built | Score BST with reverse in-order ranking, section 7 |
| Graphs for recommendations | Built | Co-borrow adjacency list, sections 7 and 8 |
| Huffman coding for storage optimization | Absent | Nothing is written to disk, so there is nothing to compress |
| Hashing for password security | Partial | The field and the sign-in path exist; no hashing function is wired in |
| User-friendly interface | Built | `LibraryUI`, role-gated menus, named refusals |
| Documentation for developers and end users | Built | `TECHNICAL_REPORT.md` and `USER_GUIDE.md` |

Four of sixteen fell short at audit time. Two were built in Module 9. Two
remain, both documented in `TECHNICAL_REPORT.md` section 9 with the reason.

---

## Module 1 project plan

| Commitment | Status | Note |
|---|---|---|
| C++17, single-user console application | Met | |
| Book inventory, accounts, borrow/return, recommendations | Met | |
| Data persists to flat files | Not met | All state is in memory and lost on exit |
| Passwords stored as salted hashes | Not met | Field only |
| Huffman coding on the archived transaction log | Not met | No log, no archive |
| Out of scope: networking, concurrency, payments, GUI | Held | Still out of scope, and the report says why |
| Heap invalidation risk, mitigated by a loan ID to index map | Met | This is exactly what was built, and it is what makes early returns O(log n) |
| Hash degradation risk, mitigated by 0.75 load factor and doubling | Met | Section 2 measures it: 5,000 prefixed ISBNs, longest chain 5 |
| Inverted index for partial title queries, built in Module 3 | Not met | Never built. Title search is still linear |
| Freeze inventory and account interfaces after Module 2 | Broadly met | `LibrarySystem` gained methods in Modules 5 and 9; the structure APIs stayed stable |
| g++ with `-Wall -Wextra` | Met | Plus `-Wpedantic`, ASan and UBSan |
| Catch2 for unit tests | Not met | Hand-written check helper instead |
| Git, one branch per module, tagged | Not met | Per-module archives instead |
| Python generator for 1,000 and 100,000 record catalogs | Not met | Stress tests generate 5,000 books in C++ |
| PlantUML diagrams versioned in the repo | Not met | Produced for the Module 2 deck; sources not in this package |
| Doxygen comments in headers | Not met | Plain block comments |
| Open Library data | Not met | Hand-written six-title catalog |
| Definition of done: 100,000 records under two seconds | Not measured | Largest test is 5,000 |
| Definition of done: correct overdue ordering | Met | Section 5 |
| Definition of done: recommendations for a user with three loans | Partial | Recommendations depend on co-borrow edges, not loan count. A patron with three loans nobody else has borrowed gets nothing. The phrasing in the plan was wrong about how the feature works |
| Definition of done: ships with a user manual and technical reference | Met | |

---

## Module 2 design document

| Commitment | Status | Note |
|---|---|---|
| Four-layer architecture, no structure calls another structure | Met | Enforced throughout; the report states the rule |
| Hash table, linked list, heap, BST, graph selections | Met | All five as designed |
| Reservation queue per title | Built (Module 9) | |
| `activeHolds` counter on `Book` | Deliberately not built | The queue knows its length in O(1). A second copy of that number is the exact shape of the two Module 6 defects. Deviation recorded in `ReservationQueue.h` |
| Fines, with a checkout threshold | Absent | |
| Renewals, blocked by a waiting hold | Absent | The hold half now exists |
| UI mock-ups: `[H] place hold` | Built (Module 9) | |
| UI mock-ups: `[E] renew`, fines line | Absent | Options left off the menu rather than shown and refused |

---

## What this audit changed

1. Built `ActivityLog` and `ReservationQueue`, plus interface screens and
   74 checks.
2. Completed the gap table in `TECHNICAL_REPORT.md` section 9, which was
   missing the title index, publication-date sorting, persistence, password
   hashing, and every tooling claim above.
3. Corrected the Module 1 definition-of-done claim about recommendations,
   which described a behavior the feature does not have.

The honest summary: the data structures at the core of the course are all
built and tested. The gaps are at the edges, in persistence, security, and
the tooling the plan named, and they are now written down rather than left
for a reader to discover.
