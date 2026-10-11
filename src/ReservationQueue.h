// ReservationQueue.h - one FIFO hold queue per title.
//
// A queue makes first-asked-first-served a property of the structure rather
// than something the surrounding code must maintain. Linked nodes with a tail
// pointer, so joining and being served are both O(1); cancelling leaves from
// the middle and is O(k).
//
// The Module 2 design put an activeHolds counter on Book. It is not here: the
// queue already knows its length in O(1), and the two worst defects found in
// Module 6 were both duplicated state disagreeing with itself.

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
