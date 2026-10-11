# Demo Script — Smart Library Management System

CPSC 315 | Michael Pelitera | Final submission

Run with `./demo/run_demo.sh` from the project root. Every run produces
identical output, so this script can be rehearsed against the real thing.
Add `--fast` to verify it still works without waiting through the pacing.

Nine scenes. Total running time at default pace: about four minutes.

---

## Before the room

- [ ] `g++ -std=c++17 -Wall -Wextra -o slms_app src/app.cpp src/LibraryUI.cpp src/BookInventory.cpp src/UserAccounts.cpp src/DueDateHeap.cpp src/Recommender.cpp src/LibrarySystem.cpp src/DemoFixture.cpp src/ActivityLog.cpp src/ReservationQueue.cpp`
- [ ] `./demo/run_demo.sh --fast` — confirm it reaches "Session ended."
- [ ] Terminal at 80 columns minimum. Every screen is drawn to 56 columns,
      so anything narrower wraps and the boxes break.
- [ ] Font large enough to read from the back. Test it from there, not from
      the keyboard.
- [ ] Have `demo/recorded_run.txt` open in a second window as the fallback
      if the live run fails.

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
cannot do, so it scans the catalog. That is acceptable at this size, and the
technical report names an inverted index as the fix if it grows. The Module 1
plan called for that index and it was never built, which is recorded in the
audit rather than left out.

---

## Scene 2 — A refusal that explains itself (about 30 seconds)

**Screen:** Same search, picks result 2, gets "All copies are currently
checked out."

**Say:** "This is the part worth watching. The system does not say 'error'
or 'operation failed'. Every rejection carries the specific rule that
stopped it, because the function returns a named status rather than a
true-or-false."

**Point at:** the message. "There are ten of these now. Loan limit
reached, no copies available, book not found, account not found, not checked
out, and the reservation cases. The interface can only tell the patron which
one applies if the logic layer distinguishes them."

---

## Scene 3 — Placing a reservation (about 30 seconds)

**Screen:** Reserve menu lists only titles with every copy out. Patron
reserves The Apollo Guidance Computer and is told they are number 1 in line.

**Say:** "The patron could not borrow it, so they join the hold list. The
screen only offers titles that are actually out, because reserving something
on the shelf is refused by the logic layer and there is no reason to offer
an action that will be rejected."

**Point at:** the position in line. "That number comes from a FIFO queue, one
per title. Joining and being served are both constant time. The ordering is a
property of the structure rather than something the surrounding code has to
remember to maintain."

---

## Scene 4 — Recommendations (about 30 seconds)

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

**If asked about privacy:** the graph holds title-to-title weights with no
user IDs in it at all, and a patron's borrowed list is cleared when they
return a book. No reading history survives a return. Section 8 of the
technical report covers what that costs and what it does not protect.

---

## Scene 5 — Account view (about 20 seconds)

**Screen:** "Active loans: 1 of 5" and the title held.

**Say:** "The limit is visible before the patron hits it, not after."

---

## Scene 6 — Activity and undo (about 30 seconds)

**Screen:** The patron's recent activity, newest first: the reservation, then
the borrow. The script chooses Back rather than undoing.

**Say:** "Every state change is pushed onto a stack for that account. Newest
first is the order the stack already has, so displaying it costs nothing."

**Point at:** the undo line. "Only the most recent action can be reversed.
That restriction comes from the stack, and it is the reason to use one here:
an undo cannot reach past an action that something else has since depended
on. The case this exists for is a staff member checking in a pile of books
who scans one twice and needs to back it out."

**If asked to run it live:** choosing 1 reverses the reservation. The test
driver covers the interesting failure too: if you undo a return and someone
else has taken the last copy in the meantime, the undo fails and the entry
goes back on the stack, because the log has to keep describing what actually
happened.

---

## Scene 7 — Librarian overdue report (about 40 seconds)

**Screen:** Signs out, signs in as librarian, two overdue items at 9 and 4
days late.

**Say:** "Different role, different menu. The patron account never showed
these options at all, so nobody picks something that gets refused. The
permission check still runs in the logic layer. Hiding an option is
presentation, not security."

**Point at:** the ordering. "Soonest due first. That comes from a min-heap
keyed on due date, so finding overdue items never scans every active loan."

**If asked about early returns:** a plain heap cannot remove an item from
the middle without searching the whole array, and early returns are the most
common write the system has. A map from loan ID to heap position fixes it.
That is in the technical report.

---

## Scene 8 — Inventory report (about 20 seconds)

**Screen:** Catalog by author, then the summary line.

**Say:** "Those last two numbers have to agree. If they ever disagree,
something is wrong, and that is exactly the class of bug testing caught in
Module 6."

---

## Scene 9 — Hold lists (about 20 seconds)

**Screen:** The staff view of every title with someone waiting, showing the
reservation placed back in scene 3.

**Say:** "Staff see the lines in the order patrons will be served. When that
copy comes back, it goes to the top of this list automatically rather than
onto the shelf."

**If asked what happens if that patron is at their loan limit:** the hold is
dropped and the copy stays on the shelf. A real system would hold it on a
shelf with an expiry and notify them. That needs a notification mechanism and
a third copy state, and the report says so rather than pretending otherwise.

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
