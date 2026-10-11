# Feedback and what changed

CPSC 315 | Michael Pelitera | Module 9

The Module 9 deliverable asks for a final submission that incorporates
feedback. This file records the feedback received during the project and what
each item actually changed, so the claim can be checked rather than taken on
trust.

---

## Instructor, Module 8 discussion (Oct 2)

> How might you structure your test fixtures in future iterations to verify
> that mock data covers all dynamic interface states automatically?

**What I answered at the time.** Build the fixture around the interface states
to be demonstrated rather than writing sample data and checking the screens
afterward, add automated checks that those conditions hold after the fixture
loads, and keep separate fixtures for the empty states.

**What changed.** That answer is now implemented rather than planned.

- `src/DemoFixture.h/.cpp` is new. Seed data moved out of `LibraryUI`, which
  the test driver does not link, into a module both binaries share. The
  original problem was structural: the fixture lived where no test could
  reach it.
- The dynamic screens are enumerated in a `Screen` enum. Section 11 of the
  test driver walks `allScreens()` rather than a hand-written list, so adding
  a screen without extending the coverage check is not possible.
- Three fixtures each declare the state they exist to produce:
  `buildDemoLibrary` for the populated case, `buildEmptyHistoryLibrary` for
  the exact Module 8 failure condition, and `buildEmptyLibrary` for an empty
  catalog.
- `screenCoverage()` replays the scripted demo path before measuring, because
  the account and recommendation screens are legitimately empty until the
  patron borrows something. The check measures what the audience sees.

**Verification.** Reintroducing the original defect, by cutting the borrowing
history out of `buildDemoLibrary`, drops the suite from 163 of 163 to 157 of
163 and names the unavailable-title, recommendation, overdue, and hold-list
screens. Before section 11 existed, the same change passed all 72 tests.

---

## Instructor, Module 9 discussion prompt (Oct 6)

> ...looking ahead to personalization features, what architectural trade-offs
> or privacy safeguards do you believe are essential when leveraging user
> borrowing history for book recommendations?

**What changed.** `TECHNICAL_REPORT.md` section 8 is new. Answering the
question honestly required reading the code rather than reasoning about the
design, and what that turned up was not what I expected: the system retains
almost no personal reading data. The co-borrowing graph stores title-to-title
weights with no user IDs, and `User::borrowedIsbns` is cleared on return, so
no per-patron history survives a book going back.

The section records that as a property the data model happens to have rather
than as a deliberate safeguard, since a later change adding a reading-history
field for better recommendations would remove it without anyone noticing. It
also names the cost (every empty shelf is a cold start), the residual
re-identification risk in weight-1 edges, and the safeguards a real
deployment would still need.

---

## Instructor, repeated across Modules 7 and 8

The same question about handling audience questions on edge cases and system
constraints appeared three times in the discussion board, to the class and to
individual students, and went unanswered by anyone.

**What changed.** The Module 8 presentation carries a limitations slide, and
`TECHNICAL_REPORT.md` separates design constraints from features that were
specified in Modules 1 and 2 but never built. The second list exists so the
gap between the design documents and the implementation is visible to a
reader rather than discovered by searching the source.

---

## Module 4 submission

The initial post was submitted without the module deliverable attached.

**What changed.** Process rather than code. Every deliverable from Module 5
onward was packaged before the post was written, and the post was written to
describe what the package contains.

---

## Self-audit against the project requirements (Module 9)

Before submitting, every claim made across Modules 1 to 8 was checked against
the build. `AUDIT.md` records the result in full. It found four data
structures named in the project's key features list that were not present.

**What changed.** Two of them were built: the activity log stack and the
reservation queue, with 74 new checks covering them. The other two, Huffman
coding and password hashing, are documented in `TECHNICAL_REPORT.md` section
9 with the reason each is absent. The gap table there was also incomplete; it
now includes the secondary title index, publication-date sorting, persistence,
and the Module 1 tooling claims that were never used.

**Test count across Module 9:** 72 → 163.

---

## Found during final review, not from feedback

**Signed integer overflow in the test driver.** The pseudo-random generator
seeding the 2,000-loan heap stress test multiplied in `int` and overflowed on
its first step, which is undefined behavior. UndefinedBehaviorSanitizer
reported it at `main.cpp:233`. Fixed by moving the arithmetic to `uint32_t`.

Recorded here because the Module 7 technical report claimed the build was
clean under UndefinedBehaviorSanitizer, and that claim was wrong when it was
written. The sanitizer run is now part of build verification rather than
something done once and cited afterward.
