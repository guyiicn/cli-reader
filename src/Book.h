#ifndef BOOK_H
#define BOOK_H

#include <string>
#include <vector>

struct Book {
    int id = 0;
    std::string title;
    std::string author;
    std::string path;
    std::string hash;
    int total_pages = 0;
    int current_page = 0;
    std::string last_read_date;
};

#endif // BOOK_H
