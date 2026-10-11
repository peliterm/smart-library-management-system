# Smart Library Management System - Module 5 Deliverable

CPSC 315 | Michael Pelitera
Advanced features: borrowing and returning with deadline management, plus a
recommendation engine. Built on the Module 3/4 inventory and account modules.

## Build and run

```
g++ -std=c++17 -Wall -Wextra -o slms src/*.cpp
./slms
```

The executable runs a test driver covering all modules. Expected output ends
with `60 of 60 checks passed`.

## Files

| File | Contents |
|------|----------|
| `src/Book.h` | Book record. One record per ISBN, with copy counts. |
| `src/BookInventory.h/.cpp` | Hash table keyed on ISBN, separate chaining, FNV-1a hash, resize at 0.75 load factor. |
| `src/User.h` | Account record and Role enumeration. |
| `src/UserAccounts.h/.cpp` | Sorted doubly linked list by user ID, paired with a hash index mapping ID to node. |
| `src/DueDateHeap.h/.cpp` | Binary min-heap on due date with a loan ID to array index map, making arbitrary removal O(log n) instead of O(n). |
| `src/Recommender.h/.cpp` | Co-borrowing graph (adjacency list, 50-neighbor cap) plus a score BST read by reverse in-order traversal. |
| `src/LibrarySystem.h/.cpp` | Integration layer. Checkout, return, overdue notices, recommendations, with validation before any write. |
| `src/main.cpp` | Test driver. 60 checks across nine sections. |

## Design decisions

**Hash function.** FNV-1a rather than a character sum. ISBN-13 values share
long prefixes, so an accumulating hash clusters records into a narrow band of
buckets. Test section 2 loads 5,000 ISBNs that all share a `9780262` prefix
and confirms the longest chain stays at 5.

**Two structures for accounts.** The linked list keeps accounts in sorted ID
order and allows insertion and removal without shifting records. The hash
index maps user ID to node pointer, which turns lookup from O(n) traversal
into O(1) average and makes deletion O(1) since no search is needed to find
the node. The risk is the two disagreeing, so `UserAccounts::validate()`
verifies ordering, back pointers, counts, and index agreement.

**Insertion cost.** User IDs are assigned in increasing order, so new accounts
almost always append at the tail in O(1). The interior insert path exists for
out-of-order IDs and is O(n) worst case.

**Integration.** The two modules never touch each other's data. Every
cross-module operation runs through `LibrarySystem`, validates completely
before the first write, and returns a named `Status` rather than a bare
false so the interface can report which rule stopped the operation.

**Deadline management.** A plain heap gives O(log n) push and pop-min, which
covers the overdue report, but it cannot locate an arbitrary element without
an O(n) scan. Books get returned early constantly, so the heap keeps a hash
map from loan ID to array index, updated inside every sift. Removal becomes
locate in O(1), swap with the last element, sift: O(log n). Test section 6
loads 2,000 loans, removes every third by ID, and re-validates.

**Recommendations.** Co-borrowing is stored as an adjacency list rather than
a matrix, since most book pairs share no borrower and a matrix would allocate
n^2 slots for mostly zeros. Each title keeps its 50 heaviest edges, which
bounds the two-hop walk no matter how popular a book becomes. Candidates are
inserted into a BST keyed on (score, ISBN); the ISBN breaks ties so equal
scores get a deterministic position. Reverse in-order traversal reads the
tree highest score first and stops once the requested count is reached.

**Validation.** Each module owns an invariant check, following the BST
validity checker pattern from Lab 7.15. `UserAccounts::validate()` covers
list ordering and index agreement, `DueDateHeap::validate()` covers the heap
property and the index map, and `Recommender::validateTree()` checks BST keys
against inherited ancestor bounds rather than only the immediate parent.
`LibrarySystem::validateAll()` runs all three.

## Verification

- Compiles clean under `-Wall -Wextra`
- AddressSanitizer and UndefinedBehaviorSanitizer report no leaks or UB
