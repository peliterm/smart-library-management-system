// Book.h - one catalog title. Copy counts are integers on the record, so a
// Book represents an ISBN rather than a physical copy.

#ifndef BOOK_H
#define BOOK_H

#include <string>

struct Book {
    std::string isbn;        // ISBN-13, the unique key for this record
    std::string title;
    std::string author;
    std::string genre;
    int year;                // publication year; 0 when unknown
    int copiesTotal;         // how many copies the library owns
    int copiesAvailable;     // how many are currently on the shelf

    Book()
        : year(0), copiesTotal(0), copiesAvailable(0) {}

    // year is last and defaults to 0 so records created before publication
    // dates existed still compile. A record with year 0 sorts after every
    // dated record rather than pretending to be from antiquity.
    Book(const std::string& isbn_, const std::string& title_,
         const std::string& author_, const std::string& genre_,
         int copies, int year_ = 0)
        : isbn(isbn_), title(title_), author(author_), genre(genre_),
          year(year_), copiesTotal(copies), copiesAvailable(copies) {}

    bool isAvailable() const { return copiesAvailable > 0; }
};

#endif // BOOK_H
