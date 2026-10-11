# Smart Library Management System — Module 7 Deliverable

CPSC 315 | Michael Pelitera
Complete documentation set plus interface refinements.

## Documents

| File | Audience |
|------|----------|
| `USER_GUIDE.md` | End users — patrons and library staff |
| `TECHNICAL_REPORT.md` | Developers — architecture, structures, costs, limitations |

## Build

```
# interactive application
g++ -std=c++17 -Wall -Wextra -o slms_app \
    src/app.cpp src/LibraryUI.cpp src/BookInventory.cpp \
    src/UserAccounts.cpp src/DueDateHeap.cpp \
    src/Recommender.cpp src/LibrarySystem.cpp

# test driver
g++ -std=c++17 -Wall -Wextra -o slms_tests \
    src/main.cpp src/BookInventory.cpp src/UserAccounts.cpp \
    src/DueDateHeap.cpp src/Recommender.cpp src/LibrarySystem.cpp
```

`./slms_app` runs the interface. `./slms_tests` runs 72 checks and should end
with `72 of 72 checks passed`.

## Module 7 changes

**New: console interface (`LibraryUI`).** Earlier modules had a test driver
but no interface a user could operate. The UI layer is separate from
`LibrarySystem` so the logic stays testable without simulating keystrokes.

**Refinement: search and borrow in one step.** The first version displayed
search results, then required the user to return to the menu and type a
13-digit ISBN by hand. Writing that step into the user guide is what exposed
it — an instruction reading "type the number exactly as shown above"
describes work the program should be doing. Results are now numbered and
borrowable directly. Manual ISBN entry remains for staff working from paper.

**Refinement: menus grouped by frequency and role.** Patron operations appear
first; staff functions sit below a separator and are hidden from roles that
cannot use them, so no one selects an option only to be refused.

**Refinement: invalid input re-prompts.** A stray keystroke used to be able to
end a session. Input is now validated in one place and re-prompted with the
expected range.

**Refinement: empty states explain themselves.** An empty recommendation list
says why it is empty rather than printing nothing.

## Known gaps

`TECHNICAL_REPORT.md` section 8 now separates two kinds of limitation:
constraints of the current design (no persistence, single-threaded, O(n)
validity sweeps) and features that were specified in Modules 1 and 2 but
never built (holds, fines, renewals, activity log stack, reservation queue,
Huffman coding). The second list exists so the gap between the design
documents and the implementation is visible rather than implied.

## Status

- Compiles clean under `-Wall -Wextra -Wpedantic`
- ASan and UBSan report no leaks or undefined behavior
- 72 of 72 checks pass
