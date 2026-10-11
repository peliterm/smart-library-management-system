===============================
SMART LIBRARY MANAGEMENT SYSTEM
===============================

CPSC 315 | Michael Pelitera | Final project

A C++17 console application for managing books, accounts, loans,
reservations, recommendations, activity and undo, password authentication,
and catalog compression. All application data is held in memory and is
lost on exit.

CONTENTS
--------

  src/
      Application source, data structures, and the 304-check test
      suite

  Makefile
      Build, test, demo, and verification commands

  Technical report (TECHNICAL_REPORT.md)
      Architecture, algorithms, testing, privacy, limitations, project
      plan, feedback, and requirements coverage

  System design (MP_CPSC315_SystemDesign.pdf)
      Final design on pages 1-2; original Module 2 architecture, UML,
      and mock-ups on pages 3-5

  Presentation (MP_CPSC315_Presentation.pptx)
      Eight slides. The spoken walkthrough is the video

  Presentation video (Final_Project_Presentation_SLMS.mp4)
      8:16 narrated walkthrough: the slides plus a recorded live
      session

  Verification record (VERIFICATION.txt)
      Captured test results, compiler details, and check-count
      reconciliation

  demo/
      Input sequence, demo runner, and the recorded output used by
      verification

WHERE EACH REQUIRED DELIVERABLE LIVES
-------------------------------------

  Project plan: timeline, milestones, required resources
      Technical report (TECHNICAL_REPORT.md) section 13

  System design document with architectural plans and mock-ups
      MP_CPSC315_SystemDesign.pdf: final design pages 1-2, Module 2
      architecture, UML and mock-ups pages 3-5

  Functional modules for book inventory and user accounts
      src/BookInventory.*, src/UserAccounts.*

  Code properly commented for testing and debugging
      src/, all 28 files

  Advanced features linked to previous modules
      src/DueDateHeap.*, src/Recommender.*, src/ActivityLog.*,
      src/ReservationQueue.*, src/Huffman.*, src/PasswordHash.*

  A stable version free of major bugs
      make verify: 304 checks, sanitizers clean. Defects found and
      fixed are in technical report section 7

  User manual
      the user guide below

  Technical report
      TECHNICAL_REPORT.md

  Feedback incorporated from instructor and peers
      Technical report (TECHNICAL_REPORT.md) section 14

  Coverage of all six required feature groups
      Technical report (TECHNICAL_REPORT.md) section 15

BUILD AND RUN
-------------

Use a C++17 compiler and GNU Make in a Unix-like shell. The demo and
verification commands also use standard shell utilities. On Windows, these
commands require a compatible environment such as WSL; they are not
PowerShell commands.

    make
    ./slms_app

The Makefile contains the compiler options and complete source lists.

    make test       # run the test suite
    make demo       # replay the scripted demonstration
    make verify     # tests, AddressSanitizer, UndefinedBehaviorSanitizer, and demo comparisons
    make clean      # remove generated executables and verification output

make verify stops with a nonzero exit status if a required check fails.
The supplied verification record reports 304 passing checks and clean
sanitizers. It is a captured record; the command reproduces the checks
locally.

DEMONSTRATION
-------------

    sh demo/run_demo.sh          # paced demonstration
    sh demo/run_demo.sh --fast   # replay without delays

The ten scenes show search and borrowing, a refusal, reservations,
recommendations, account loans, activity and undo, overdue items,
inventory, hold lists, and compression. The same session, narrated, is in
Final_Project_Presentation_SLMS.mp4 from 1:42. demo/recorded_run.txt is
both the fallback transcript and the expected output used by make verify.

Invoked through sh because unzipping on Windows, or cloning onto a drive
that does not carry Unix permissions, drops the executable bit.

USER GUIDE
----------

Signing in

The sign-in screen asks for an account ID and a password.

    --------------------------------------------------------
     SIGN IN
    --------------------------------------------------------
     Demo accounts:
       1001  Mike       (Student)
       1002  Priya      (Student)
       2001  A. Rivera  (Librarian)
       3001  Admin      (Administrator)
    
     Password for every demo account: library
    
     Enter 0 to exit.
    
    Account ID:

A wrong password is refused without saying whether the account exists.
Enter 0 at the account prompt to close the program.

Your account has one of three roles, and the role decides what your menu
shows. A student sees seven options. A librarian or administrator sees
those seven plus five staff options. Options your role cannot use are not
displayed at all, so you will not pick something only to be told no.
Reporting and staff-management operations also check the account role in
the logic layer. Technical report section 9 explains the authorization
scope.

The main menu

Everyone sees these seven:

       1. Search and borrow
       2. Borrow by ISBN
       3. Return a book
       4. My account
       5. Recommendations for me
       6. Reserve a title that is out
       7. My recent activity
    
       8. Sign out

Sign out is option 8 for a student and option 13 for staff, because the
staff options sit between them.

Staff see five more before sign out:

  8
      Overdue report

  9
      Inventory report

  10
      Hold lists

  11
      Storage report

  12
      Manage catalog and accounts

  13
      Sign out (staff)

Searching and borrowing

Option 1 asks for part of a title or an author name. Capitalisation does
not matter and you do not need the full title. Leave it blank to list
everything.

     Title or author (blank lists everything): apollo
    
        1. Digital Apollo                  Mindell     3 available
        2. The Apollo Guidance Computer    O'Brien     all copies out
    
       2 titles found.
       3. Back to menu
    
    Select [1-3]:

Type the number beside a title to borrow it. The due date appears
immediately. A title showing "all copies out" cannot be borrowed; use
option 6 to join its hold list instead.

Option 2 takes a 13-digit ISBN directly. It exists for staff working from
a printed list. If you are reading from the screen, option 1 is faster.

Returning

Option 3 lists what you have out and numbers it. Pick the number. If
somebody is waiting on that title, the copy is offered to the first
eligible person in line, and the screen says so. A patron at the loan
limit keeps their place.

My account

Option 4 shows your role, how many loans you have against the limit of
five, and one line per loan with its own due date.

     Account 1001  Mike  (Student)
     Active loans: 1 of 5
    
       Title                             Due       Status
       Digital Apollo                    day 121   due in 21 days

If you hold two copies of the same title, each copy appears on its own
line with its own due date, because each loan is tracked separately.

Dates use demonstration day numbers rather than a live calendar.

Recommendations

Option 5 suggests titles from recorded co-borrowing connections to the
books you currently have out. A higher match score means those pairings
occurred more often; it does not count distinct borrowers.

If you have nothing out, there is nothing to suggest from, and the screen
says so rather than printing an empty list.

Reserving a title

Option 6 lists only titles with every copy checked out, since anything on
the shelf should simply be borrowed. Pick a number to join the line, and
the screen tells you your position.

       Reserved. You are number 1 in line.
       The next copy returned goes to the front of the line.

When a copy comes back it is offered to the first person waiting. If that
person is already at their five-loan limit the copy passes to the next
person, and the one who was skipped keeps their place at the front for
next time.

Recent activity and undo

Option 7 lists your recent actions, newest first, and offers to undo the
most recent one.

       1. Reserved              The Apollo Guidance Computer    day 100
       2. Borrowed              Digital Apollo                  day 100
    
       Only the most recent action can be undone.

Only the newest action can be reversed. This is deliberate: it keeps an
undo from reaching past something that has already been acted on. Undoing
a return puts the loan back with its original due date rather than
starting a fresh loan period.

One thing to know: undoing a cancelled reservation puts you at the back of
the line, not back where you were. This implementation does not restore
the previous queue position.

Staff screens

Overdue report (8). Every loan past its due date, soonest first, with the
patron's name and how many days late.

Inventory report (9). The catalog by author, with copies available against
copies owned, and totals at the end.

Hold lists (10). Every title with people waiting, in the order they will
be served.

Storage report (11). What the catalog costs to store and what compression
saves. At the demo's six titles the compressed archive is larger than the
original, because the lookup table compression needs is a fixed cost that
six books cannot pay back. The screen explains this. In the generated
5,000-book test, the archive uses about 61 percent of the original space.
Results depend on the input data.

Manage catalog and accounts (12). Add or remove a book, add or remove an
account.

A book cannot be removed while any copy is out or anybody is waiting on
it. An account cannot be removed while it holds a loan or sits in a hold
list. In both cases the system refuses and says why, rather than leaving
records pointing at something that no longer exists. You also cannot
remove the account you are signed in to.

Messages you may see

Every refusal names the specific rule that stopped it.

  All copies are currently checked out.
      What it means: Every copy of that title is with another patron.
      What to do: Use option 6 to join the hold list.

  Loan limit of 5 books reached.
      What it means: You already have five books out.
      What to do: Return something first.

  No book found with that ISBN.
      What it means: The number did not match any record.
      What to do: Check the digits, or search by title with option 1.

  No account found with that ID.
      What it means: That account number does not exist.
      What to do: Check the number with a librarian.

  That book is not on this account.
      What it means: You tried to return something you do not have
      out.
      What to do: Use option 3, which lists only what you hold.

  You are already on the hold list for this title.
      What it means: You asked twice.
      What to do: Your original place is unchanged.

  You are not on the hold list for this title.
      What it means: You tried to cancel a hold you do not have.
      What to do: Check option 7 for what you actually did.

  A copy is on the shelf. Borrow it instead of reserving.
      What it means: There is no need to wait.
      What to do: Borrow it with option 1.

  There is nothing on this account to undo.
      What it means: No recorded actions remain.
      What to do: Nothing to do.

  That action can no longer be reversed.
      What it means: Something since then depends on it, most often
      because the copy has gone to somebody else.
      What to do: Ask a librarian.

  This account does not have permission for that.
      What it means: A staff-only operation from a patron account.
      What to do: Ask a librarian.

  A record with that identifier already exists.
      What it means: That ISBN or account ID is in use.
      What to do: Check the existing record first.

  That record is still in use and cannot be removed.
      What it means: Copies are out, people are waiting, or the
      account holds loans.
      What to do: Clear those first, then remove it.

What this system does not do

Stated here so nothing surprises you.

  - Nothing is saved. Closing the program loses everything. There is no
    file
on disk.
  - One person at a time. It is not built for two librarians working at
    once.
  - No fines and no renewals. Both were designed and neither was built.
  - Dates are day numbers. There is no calendar.
