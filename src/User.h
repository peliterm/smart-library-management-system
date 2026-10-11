// ============================================================================
// User.h
// Smart Library Management System - CPSC 315
//
// Account record. Role controls which menu options the interface offers.
// passwordHash stores a hash of the password, never the password itself.
// ============================================================================
#ifndef USER_H
#define USER_H

#include <string>
#include <vector>

enum class Role { STUDENT, LIBRARIAN, ADMINISTRATOR };

inline std::string roleName(Role r) {
    switch (r) {
        case Role::STUDENT:       return "Student";
        case Role::LIBRARIAN:     return "Librarian";
        case Role::ADMINISTRATOR: return "Administrator";
    }
    return "Unknown";
}

struct User {
    int         userId;             // unique, assigned in increasing order
    std::string name;
    std::string email;
    std::string passwordHash;
    Role        role;
    std::vector<std::string> borrowedIsbns;   // ISBNs currently checked out

    User() : userId(0), role(Role::STUDENT) {}

    User(int id, const std::string& name_, const std::string& email_,
         const std::string& pwHash, Role role_)
        : userId(id), name(name_), email(email_),
          passwordHash(pwHash), role(role_) {}

    size_t activeLoanCount() const { return borrowedIsbns.size(); }
};

#endif // USER_H
