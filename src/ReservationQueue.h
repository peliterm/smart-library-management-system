// ============================================================================
// ReservationQueue.h
// Smart Library Management System - CPSC 315
//
// One FIFO queue of waiting patrons per title, which is the structure the
// project specification calls for to process book reservations.
//
// Why a queue. The rule a hold list has to enforce is that the person who
// asked first is served first. That is the definition of a queue, and using
// one makes the ordering a property of the structure rather than something
// the surrounding code has to remember to maintain.
//
// Hand-written over linked nodes with a tail pointer, so enqueue and dequeue
// are both O(1). A vector would make dequeue O(n) because every remaining
// element shifts forward, and the front of the line is the end that moves
// most often.
//
// Cancellation is the one operation that is not FIFO: a patron can leave the
// line from the middle. That is a linear walk, which is acceptable because
// cancelling is rare compared to joining and being served, and because hold
// lines are short.
//
// Note on the Module 2 design. That document specified an activeHolds counter
// stored on the Book record so the "is anyone waiting" check would be O(1).
// This implementation keeps the count inside the queue instead. The counter
// on Book would have been a second copy of a number this structure already
// knows, and the two most serious defects found in Module 6 were both cases
// of duplicated state disagreeing with itself. lengthOf() is already O(1),
// so the counter bought nothing and risked the same class of bug.
// ============================================================================
#ifndef RESERVATION_QUEUE_H
#define RESERVATION_QUEUE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>

class ReservationQueue {
public:
    ReservationQueue();
    ~ReservationQueue();

    // Non-copyable: the class owns raw node pointers.
    ReservationQueue(const ReservationQueue&) = delete;
    ReservationQueue& operator=(const ReservationQueue&) = delete;

    // Join the back of the line for a title. Returns false if this patron is
    // already waiting on it, so a double request cannot buy a second place.
    // O(1) apart from the duplicate check, which walks the line.
    bool enqueue(const std::string& isbn, int userId);

    // Take the patron at the front of the line. False when nobody waits.
    // O(1).
    bool dequeue(const std::string& isbn, int* outUserId);

    // Leave the line from wherever this patron is standing. O(k) in the
    // length of that title's line.
    bool cancel(const std::string& isbn, int userId);

    bool   isWaiting(const std::string& isbn, int userId) const;
    int    frontOf(const std::string& isbn) const;      // 0 when empty
    size_t lengthOf(const std::string& isbn) const;     // O(1)
    size_t totalWaiting() const { return total; }

    // The line in order, front first. For display.
    std::vector<int> waitingOn(const std::string& isbn) const;

    // Every title with anyone waiting, for the staff view.
    std::vector<std::string> titlesWithHolds() const;

    // Walked lengths match recorded counts, every tail really is the last
    // node, and no patron appears twice in one line.
    bool validate() const;

private:
    struct Node {
        int   userId;
        Node* next;
        explicit Node(int id) : userId(id), next(nullptr) {}
    };

    struct Line {
        Node*  head;
        Node*  tail;
        size_t count;
        Line() : head(nullptr), tail(nullptr), count(0) {}
    };

    std::unordered_map<std::string, Line> lines;
    size_t total;

    void clear();
};

#endif // RESERVATION_QUEUE_H
