// ============================================================================
// UserAccounts.h
// Smart Library Management System - CPSC 315
//
// Sorted doubly linked list of user accounts, ordered by user ID, paired with
// a hash index that maps user ID to the node holding that account.
//
// Why both structures:
//   - The linked list keeps accounts in sorted order for sequential display
//     and allows insertion and removal without shifting other records.
//   - A list alone requires O(n) traversal to locate an account. The hash
//     index removes that cost, giving O(1) average lookup while the list
//     keeps its ordering and its cheap structural edits.
//   - Because the index stores a node pointer, deletion is O(1): look up the
//     node, relink its two neighbors, free it. No traversal is needed.
//
// Insertion cost:
//   User IDs are assigned in increasing order, so a new account almost always
//   belongs at the tail. The tail pointer makes that case O(1). The general
//   insert path still walks backward from the tail to place an out-of-order
//   ID (for example, restoring a previously deleted account), which is O(n)
//   in the worst case but rare in practice.
// ============================================================================
#ifndef USER_ACCOUNTS_H
#define USER_ACCOUNTS_H

#include "User.h"
#include <string>
#include <unordered_map>
#include <vector>

class UserAccounts {
public:
    UserAccounts();
    ~UserAccounts();

    // Non-copyable: the class owns raw node pointers.
    UserAccounts(const UserAccounts&) = delete;
    UserAccounts& operator=(const UserAccounts&) = delete;

    // Inserts an account in sorted position by user ID.
    // Returns false if the ID already exists.
    bool addUser(const User& user);

    // O(1) average: index lookup, then relink neighbors.
    bool removeUser(int userId);

    // O(1) average via the index. Returns nullptr if not found.
    User* findUser(int userId);

    // Sequential access in sorted ID order, which is what the list provides
    // and the hash index cannot.
    std::vector<User> listAll() const;

    size_t size() const { return count; }

    // Verifies that the list ordering and the index agree with each other.
    // Used in testing to catch the main risk of keeping two structures.
    bool validate() const;

private:
    struct Node {
        User  user;
        Node* prev;
        Node* next;
        explicit Node(const User& u) : user(u), prev(nullptr), next(nullptr) {}
    };

    Node* head;
    Node* tail;
    size_t count;

    // Auxiliary index: user ID -> the node holding that account.
    // Every insert and remove must update this alongside the list.
    std::unordered_map<int, Node*> index;

    void clear();
};

#endif // USER_ACCOUNTS_H
