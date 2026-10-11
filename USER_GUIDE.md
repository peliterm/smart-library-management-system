# Smart Library Management System
## User Guide

CPSC 315 | Michael Pelitera | Module 7 Deliverable

---

## Before you start

You need an account ID. The system shows the available accounts on the sign-in
screen, so you do not need to memorize one.

Everything in this program is a numbered menu. You will never need to type a
command name or remember a keyword. When a screen asks for a selection, type
the number and press Enter.

If you type something the screen does not recognize, it tells you what it
expected and asks again. Nothing is lost and the program does not close.

---

## Starting the program

```
./slms_app
```

The first screen lists the demo accounts:

```
--------------------------------------------------------
 SIGN IN
--------------------------------------------------------
 Demo accounts:
   1001  Mike       (Student)
   1002  Priya      (Student)
   2001  A. Rivera  (Librarian)
   3001  Admin      (Administrator)

 Enter 0 to exit.

Account ID:
```

Type an account number and press Enter. Type `0` to close the program.

---

## The main menu

Once you sign in, the top-right corner of every screen shows your name and
role. This matters on a shared terminal, because it tells you at a glance
whose account is active.

Students and general patrons see five options:

```
   1. Search and borrow
   2. Borrow by ISBN
   3. Return a book
   4. My account
   5. Recommendations for me

   6. Sign out
```

Librarians and administrators see the same five, plus a staff section below a
separator. Options you are not permitted to use do not appear at all, so you
will never select something only to be refused.

---

## Finding and borrowing a book

**The short version:** choose `1`, type part of a title or author, then type
the number next to the book you want.

Choose **1. Search and borrow**. You are asked for a title or author:

```
 Title or author (blank lists everything):
```

You can type part of a word. Searching for `apollo` finds *Digital Apollo* and
*The Apollo Guidance Computer*. Capital letters do not matter. Leaving the box
blank and pressing Enter shows the entire catalog.

Results are numbered:

```
    1. Digital Apollo                  Mindell     3 available
    2. The Apollo Guidance Computer    O'Brien     1 available

   2 titles found.
   3. Back to menu
```

Type the number of the book you want and press Enter. That checks it out.
Type the last number to go back without borrowing anything.

On success you see the title and when it is due:

```
   Checked out: Digital Apollo
   Due in 21 days (day 121).
```

### If the book will not check out

The program always tells you the specific reason rather than a general error.
There are four possibilities:

| Message | What it means | What to do |
|---|---|---|
| All copies are currently checked out | Every copy is with another patron | Search again later, or ask staff about a hold |
| Loan limit of 5 books reached | You already have five books out | Return something first |
| No book found with that ISBN | The number was mistyped | Use **Search and borrow** instead of typing the ISBN |
| No account found with that ID | The account does not exist | Check the ID on the sign-in screen |

### Borrowing by ISBN

**2. Borrow by ISBN** exists for staff working from a printed list or an order
form. It asks for the 13-digit number directly. If you are looking at the
screen rather than paper, option 1 is faster and avoids typing mistakes.

---

## Returning a book

Choose **3. Return a book**. Your current loans appear as a numbered list:

```
 Your current loans:
   1. Digital Apollo  (9780262134729)
   2. Effective Modern C++  (9780134685991)
   3. Cancel
```

Type the number of the book you are returning. You do not type the ISBN.

If you have nothing checked out, the screen says so and returns you to the
menu.

If you hold two copies of the same title, returning once closes the older loan
first. The second copy stays on your account until you return it as well.

---

## Checking your account

**4. My account** shows your name, your role, how many books you have out, and
your limit:

```
 Account 1001  Mike  (Student)
 Active loans: 2 of 5
```

Below that is every title you currently hold. If you have nothing out, the
screen says so.

---

## Recommendations

**5. Recommendations for me** suggests titles based on what other patrons
borrowed alongside the books you have borrowed.

If the list is empty, the screen explains why rather than showing a blank
space. Recommendations need borrowing history to work from, so a brand new
account will not have any until it has checked out a few books.

Each suggestion shows a match score. A higher number means more patrons
borrowed that title together with books you have read.

---

## Staff functions

These appear only for librarian and administrator accounts.

### Overdue report

**6. Overdue report** lists every loan past its due date, soonest due first,
with the patron's name and how many days late it is:

```
   The Apollo Guidance Computer  Priya         4 days late
   Digital Apollo                Mike          1 day late

   2 overdue items.
```

If nothing is overdue, the screen says so.

### Inventory report

**7. Inventory report** lists the whole catalog grouped by author, showing
available copies against total copies, followed by a summary:

```
   5 titles, 12 copies, 3 currently out.
   Active loans tracked: 3
```

The last line is a consistency check. If the number of loans tracked ever
disagrees with the number of copies out, something is wrong and it should be
reported.

---

## Signing out

The last numbered option on the main menu signs you out and returns to the
sign-in screen. From there, `0` closes the program.

On a shared terminal, sign out when you are finished. The next person should
not act on your account.

---

## Getting help

If a screen behaves in a way this guide does not describe, note the sequence
of menu choices that led to it. That sequence is what makes a problem
reproducible, and it is the most useful thing you can provide when reporting
one.
