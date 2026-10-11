// UserAccounts.h - sorted doubly linked list of accounts plus a hash index
// from user ID to node.
//
// The list is a project requirement; alone it makes lookup O(n). The index
// gives O(1) average lookup and O(1) deletion, since the node is handed over
// directly and only its two neighbours need relinking. The cost is two
// structures that can disagree, which validate() checks.

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
