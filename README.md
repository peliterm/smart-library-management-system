# Smart Library Management System - Module 4 Deliverable

CPSC 315 | Michael Pelitera
Core feature implementation: book inventory and user account management.

## Build and run

```
g++ -std=c++17 -Wall -Wextra -o slms src/*.cpp
./slms
```

The executable runs a test driver covering all modules. Expected output ends
with `29 of 29 checks passed`.

## Files

| File | Contents |
|------|----------|
| `src/Book.h` | Book record. One record per ISBN, with copy counts. |
| `src/BookInventory.h/.cpp` | Hash table keyed on ISBN, separate chaining, FNV-1a hash, resize at 0.75 load factor. |
| `src/User.h` | Account record and Role enumeration. |
| `src/UserAccounts.h/.cpp` | Sorted doubly linked list by user ID, paired with a hash index mapping ID to node. |
| `src/LibrarySystem.h/.cpp` | Integration layer. Checkout and return, with validation before any write. |
| `src/main.cpp` | Test driver. 29 checks across the four areas. |

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

## Verification

- Compiles clean under `-Wall -Wextra`
- AddressSanitizer and UndefinedBehaviorSanitizer report no leaks or UB
