#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include "Book.h"
#include <string>
#include <vector>
#include <memory>

// Forward declaration for the sqlite3 struct
struct sqlite3;

class DatabaseManager {
public:
    DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    bool InitDatabase();
    bool AddBook(const Book& book);
    bool BookExists(const std::string& hash);
    std::vector<Book> GetAllBooks();
    bool UpdateProgress(int book_id, int current_page);
    bool UpdateLastReadTime(int book_id);
    bool DeleteBook(int book_id);
    bool GetBookPath(int book_id, std::string& path);

private:
    std::string db_path_;
    sqlite3* db_ = nullptr;
};

#endif // DATABASE_MANAGER_H
