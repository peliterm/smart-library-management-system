# Demo Script — Smart Library Management System

CPSC 315 | Michael Pelitera | Module 8

Run with `./demo/run_demo.sh` from the project root. Every run produces
identical output, so this script can be rehearsed against the real thing.
Add `--fast` to verify it still works without waiting through the pacing.

Total running time at default pace: about 3 minutes.

---

## Before the room

- [ ] `g++ -std=c++17 -Wall -Wextra -o slms_app src/app.cpp src/LibraryUI.cpp src/BookInventory.cpp src/UserAccounts.cpp src/DueDateHeap.cpp src/Recommender.cpp src/LibrarySystem.cpp`
- [ ] `./demo/run_demo.sh --fast` — confirm it reaches "Session ended."
- [ ] Terminal at 80 columns minimum. Every screen is drawn to 56 columns,
      so anything narrower wraps and the boxes break.
- [ ] Font large enough to read from the back. Test it from there, not from
      the keyboard.
- [ ] Have `/tmp/demo_out.txt` (a captured run) open in a second window as
      the fallback if the live run fails.

---

## Scene 1 — Search and borrow (about 40 seconds)

**Screen:** Student signs in, searches "apollo", picks result 1.

**Say:** "A patron searches for part of a title. They do not need the ISBN,
and capitalization does not matter. Results come back numbered, and typing
the number borrows the book. The due date is shown immediately."

**Point at:** the availability column. "Digital Apollo has three copies.
The Apollo Guidance Computer has one and it is already out. The system says
so before the patron tries."

**If asked how the search works:** exact ISBN lookup is a hash table at O(1)
average. This is a substring match across title and author, which hashing
cannot do, so it scans the catalog. That is acceptable at this size and the
technical report names an inverted index as the fix if it grows.

---

## Scene 2 — A refusal that explains itself (about 30 seconds)

**Screen:** Same search, picks result 2, gets "All copies are currently
checked out."

**Say:** "This is the part worth watching. The system does not say 'error'
or 'operation failed'. Every rejection carries the specific rule that
stopped it, because the function returns a named status rather than a
true-or-false."

**Point at:** the message. "There are five of these. Loan limit reached, no
copies available, book not found, account not found, not checked out. The
interface can only tell the patron which one applies if the logic layer
distinguishes them."

---

## Scene 3 — Recommendations (about 30 seconds)

**Screen:** Two recommendations with match scores, 2 and 1.

**Say:** "These come from co-borrowing. Two other patrons borrowed A Brief
History of Time alongside Digital Apollo, so it scores 2. One borrowed
Effective Modern C++ alongside it, so that scores 1."

**Point at:** the ordering. "A graph finds the candidates, because a graph
can represent arbitrary connections between titles. A tree ranks them,
because a graph has no ordering. Highest score first comes from a reverse
in-order traversal."

**If asked about scale:** each title keeps only its 50 strongest connections,
so a heavily borrowed book cannot end up linked to the entire catalog.

---

## Scene 4 — Account view (about 20 seconds)

**Screen:** "Active loans: 1 of 5" and the title held.

**Say:** "The limit is visible before the patron hits it, not after."

---

## Scene 5 — Librarian overdue report (about 40 seconds)

**Screen:** Signs out, signs in as librarian, two overdue items at 9 and 4
days late.

**Say:** "Different role, different menu. The patron account never showed
these options at all, so nobody picks something that gets refused. The
permission check still runs in the logic layer — hiding an option is
presentation, not security."

**Point at:** the ordering. "Soonest due first. That comes from a min-heap
keyed on due date, so finding overdue items never scans every active loan."

**If asked about early returns:** a plain heap cannot remove an item from
the middle without searching the whole array, and early returns are the most
common write the system has. A map from loan ID to heap position fixes it.
That is in the technical report.

---

## Scene 6 — Inventory report (about 20 seconds)

**Screen:** Catalog by author, then the summary line.

**Say:** "Six titles, fourteen copies, four out, four loans tracked. Those
last two numbers have to agree. If they ever disagree, something is wrong,
and that is exactly the class of bug testing caught in Module 6."

---

## Closing line

"Everything shown ran from one build with no edits between scenes. The same
script runs the same way every time, which is how I rehearsed it."

---

## If the demo fails live

1. Do not debug on stage. Say "I have a recorded run" and switch windows.
2. The captured output in the second window covers every scene in order.
3. Offer to run it live again at the end if there is time.

A demo that fails and recovers in five seconds costs almost nothing. A demo
that fails and gets debugged in front of the room costs the rest of the
presentation.
