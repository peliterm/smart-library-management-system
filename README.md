# Smart Library Management System — Final Submission

CPSC 315 | Michael Pelitera | Module 9

A console library management system in C++17. Book inventory on a hash table,
user accounts on a sorted linked list with an index, borrowing and returning
against an indexed min-heap of due dates, and recommendations from a
co-borrowing graph ranked through a binary search tree.

## What is here

| Path | Purpose |
|------|---------|
| `src/` | Full source, nine modules plus the interface and test driver |
| `TECHNICAL_REPORT.md` | Architecture, structures and costs, verification, privacy, limitations |
| `USER_GUIDE.md` | End users — patrons and library staff |
| `MP_CPSC315_Module8_Presentation.pptx` | Eight-slide presentation |
| `demo/DEMO_SCRIPT.md` | Scene-by-scene narration, pre-flight checklist, failure plan |
| `demo/run_demo.sh` | Replays the demo deterministically |
| `demo/demo_input.txt` | The fixed keystroke sequence |
| `demo/recorded_run.txt` | Captured run, used as the live fallback |
| `FEEDBACK.md` | Instructor feedback received and what changed because of it |
| `AUDIT.md` | Every claim made across Modules 1 to 8, checked against the build |

## Build

```
# interactive application
g++ -std=c++17 -Wall -Wextra -o slms_app \
    src/app.cpp src/LibraryUI.cpp src/BookInventory.cpp \
    src/UserAccounts.cpp src/DueDateHeap.cpp \
    src/Recommender.cpp src/LibrarySystem.cpp src/DemoFixture.cpp \
    src/ActivityLog.cpp src/ReservationQueue.cpp

# test driver
g++ -std=c++17 -Wall -Wextra -o slms_tests \
    src/main.cpp src/BookInventory.cpp src/UserAccounts.cpp \
    src/DueDateHeap.cpp src/Recommender.cpp src/LibrarySystem.cpp \
    src/DemoFixture.cpp src/ActivityLog.cpp src/ReservationQueue.cpp
```

`./slms_app` runs the interface. `./slms_tests` runs 163 checks and should end
with `163 of 163 checks passed`.

## Running the demo

```
./demo/run_demo.sh           # readable pace, about 3 minutes
./demo/run_demo.sh --fast    # no delay, for verification
```

Nine scenes: search and borrow, a refusal with a named reason, placing a
reservation, recommendations, the account view, the activity stack with its
undo, the overdue report, the inventory report, and the staff hold lists.
Three consecutive runs produce byte-identical output.

## Module 9 changes

**Activity log stack and reservation queue built.** Both are named in the
project's key features list and were missing. `src/ActivityLog.h/.cpp` is a
linked stack per account, pushed on every state change, with an undo that
reverses the most recent action. `src/ReservationQueue.h/.cpp` is a linked
FIFO queue per title with a tail pointer, so joining and being served are
both O(1). Returning a copy that someone is waiting on hands it to the front
of the line rather than putting it back on the shelf.

Both carry their own validity check, and `validateAll()` now runs five rather
than three.

One deliberate deviation from the Module 2 design: that document put an
`activeHolds` counter on `Book`. The queue already knows its own length in
O(1), and the two worst defects found in Module 6 were both duplicated state
disagreeing with itself, so the counter was left out.

**Demo fixture is now testable.** The seed data used to live inside
`LibraryUI`, which the test driver does not link, so no test could see it.
That is how the Module 8 dry run ended up with two empty screens while all 72
tests passed. The fixture moved to `src/DemoFixture.h/.cpp`, the dynamic
screens are enumerated there, and section 11 of the test driver asserts that
the fixture reaches the populated state of every one of them. Empty states
get their own fixtures rather than being covered by accident.

Verified by reintroducing the original defect: removing the borrowing history
from `buildDemoLibrary` drops the suite to 157 of 163 and names the four
screens that go blank.

**Signed overflow fixed in the stress-test generator.** The pseudo-random
generator feeding section 6 multiplied in `int` and overflowed by design,
which is undefined behavior. UndefinedBehaviorSanitizer flagged it during
final review. The arithmetic is now done in `uint32_t`, where wrapping is
defined.

**Privacy analysis of the recommendation data.** `TECHNICAL_REPORT.md`
section 8 documents what the system actually retains about borrowing, which
turns out to be less than expected: the co-borrowing graph holds title-to-
title weights with no user IDs, and `borrowedIsbns` is cleared on return, so
no per-patron reading history survives. The section records this as a
property the data model happens to have rather than a deliberate safeguard,
names the cold-start cost it imposes, and lists what a real deployment would
still need.

## Module 8 changes

**Demo seed data.** The previous seed had a catalog and accounts but no
borrowing history, so the recommendation engine had no co-borrow edges to
walk and the overdue report was empty. Two of the system's features demoed as
blank screens. The seed now builds real activity and advances the clock, so
recommendations return scored results and the overdue report shows two items
at 9 and 4 days late.

**Scripted demo.** Keystrokes live in a file rather than in the presenter's
hands. This removes typing mistakes as a failure mode and makes the demo
rehearsable against the exact output it will produce.

## Module 7 changes

**New: console interface (`LibraryUI`).** Earlier modules had a test driver
but no interface a user could operate. The UI layer is separate from
`LibrarySystem` so the logic stays testable without simulating keystrokes.

**Refinement: search and borrow in one step.** The first version displayed
search results, then required the user to return to the menu and type a
13-digit ISBN by hand. Writing that step into the user guide is what exposed
it: an instruction reading "type the number exactly as shown above" describes
work the program should be doing. Results are now numbered and borrowable
directly. Manual ISBN entry remains for staff working from paper.

**Refinement: menus grouped by frequency and role.** Patron operations appear
first; staff functions sit below a separator and are hidden from roles that
cannot use them, so no one selects an option only to be refused.

**Refinement: invalid input re-prompts.** A stray keystroke used to be able to
end a session. Input is now validated in one place and re-prompted with the
expected range.

**Refinement: empty states explain themselves.** An empty recommendation list
says why it is empty rather than printing nothing.

## Known gaps

`TECHNICAL_REPORT.md` section 9 separates two kinds of limitation:
constraints of the current design (no persistence, single-threaded, O(n)
validity sweeps) and features that were specified in Modules 1 and 2 but
never built (holds, fines, renewals, activity log stack, reservation queue,
Huffman coding). The second list exists so the gap between the design
documents and the implementation is visible rather than implied.

## Status

- Compiles clean under `-Wall -Wextra -Wpedantic`
- ASan and UBSan report no leaks or undefined behavior
- 163 of 163 checks pass
- Three consecutive demo runs byte-identical, and identical to the recorded
  fallback (MD5 `cf412e67bd1c236d948f414c6327f15b`)
