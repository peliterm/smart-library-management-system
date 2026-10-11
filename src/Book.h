// ============================================================================
// Book.h
// Smart Library Management System - CPSC 315
//
// Record type for a single catalog title. One Book represents one ISBN, not
// one physical copy, so copy counts are stored as integers on the record.
// ============================================================================
#ifndef BOOK_H
#define BOOK_H

#include <string>

struct Book {
    std::string isbn;        // ISBN-13, the unique key for this record
    std::string title;
    std::string author;
    std::string genre;
    int copiesTotal;         // how many copies the library owns
    int copiesAvailable;     // how many are currently on the shelf

    Book()
        : copiesTotal(0), copiesAvailable(0) {}

    Book(const std::string& isbn_, const std::string& title_,
         const std::string& author_, const std::string& genre_,
         int copies)
        : isbn(isbn_), title(title_), author(author_), genre(genre_),
          copiesTotal(copies), copiesAvailable(copies) {}

    bool isAvailable() const { return copiesAvailable > 0; }
};

#endif // BOOK_H
