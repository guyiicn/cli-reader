#include "DatabaseManager.h"
#include "DebugLogger.h"
#include "sqlite3.h"
#include <iostream>

DatabaseManager::DatabaseManager(const std::string& db_path) : db_path_(db_path) {
    if (sqlite3_open(db_path.c_str(), &db_)) {
        DebugLogger::log("Can't open database: " + std::string(sqlite3_errmsg(db_)));
        db_ = nullptr;
    } else {
        DebugLogger::log("Opened database successfully: " + db_path);
    }
}

DatabaseManager::~DatabaseManager() {
    if (db_) {
        sqlite3_close(db_);
        DebugLogger::log("Closed database.");
    }
}

bool DatabaseManager::InitDatabase() {
    if (!db_) return false;

    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS books (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            author TEXT,
            path TEXT NOT NULL UNIQUE,
            hash TEXT NOT NULL UNIQUE,
            total_pages INTEGER DEFAULT 0,
            current_page INTEGER DEFAULT 0,
            added_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            last_read_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )";

    char* err_msg = nullptr;
    if (sqlite3_exec(db_, sql, 0, 0, &err_msg) != SQLITE_OK) {
        DebugLogger::log("Failed to create table: " + std::string(err_msg));
        sqlite3_free(err_msg);
        return false;
    }
    
    DebugLogger::log("Table 'books' created or already exists.");
    return true;
}

bool DatabaseManager::AddBook(const Book& book) {
    if (!db_) return false;

    const char* sql = "INSERT INTO books (title, author, path, hash, total_pages, current_page) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        DebugLogger::log("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    sqlite3_bind_text(stmt, 1, book.title.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, book.author.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, book.path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, book.hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, book.total_pages);
    sqlite3_bind_int(stmt, 6, book.current_page);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        DebugLogger::log("Failed to execute statement: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    DebugLogger::log("Successfully added book: " + book.title);
    return true;
}

bool DatabaseManager::BookExists(const std::string& hash) {
    if (!db_) return false;

    const char* sql = "SELECT 1 FROM books WHERE hash = ?;";
    sqlite3_stmt* stmt;
    bool exists = false;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = true;
        }
    } else {
        DebugLogger::log("Failed to prepare statement for BookExists: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_finalize(stmt);
    return exists;
}

std::vector<Book> DatabaseManager::GetAllBooks() {
    std::vector<Book> books;
    if (!db_) return books;

    const char* sql = "SELECT id, title, author, path, hash, total_pages, current_page, last_read_date FROM books ORDER BY last_read_date DESC;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        DebugLogger::log("Failed to prepare statement for GetAllBooks: " + std::string(sqlite3_errmsg(db_)));
        return books;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Book book;
        book.id = sqlite3_column_int(stmt, 0);
        book.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        book.author = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        book.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        book.hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        book.total_pages = sqlite3_column_int(stmt, 5);
        book.current_page = sqlite3_column_int(stmt, 6);
        book.last_read_date = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        books.push_back(book);
    }

    sqlite3_finalize(stmt);
    return books;
}

bool DatabaseManager::UpdateProgress(int book_id, int current_page) {
    if (!db_) return false;

    const char* sql = "UPDATE books SET current_page = ? WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        DebugLogger::log("Failed to prepare statement for UpdateProgress: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    sqlite3_bind_int(stmt, 1, current_page);
    sqlite3_bind_int(stmt, 2, book_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        DebugLogger::log("Failed to execute UpdateProgress: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    DebugLogger::log("Updated progress for book_id " + std::to_string(book_id));
    return true;
}

bool DatabaseManager::UpdateLastReadTime(int book_id) {
    if (!db_) return false;

    const char* sql = "UPDATE books SET last_read_date = CURRENT_TIMESTAMP WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        DebugLogger::log("Failed to prepare statement for UpdateLastReadTime: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    sqlite3_bind_int(stmt, 1, book_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        DebugLogger::log("Failed to execute UpdateLastReadTime: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool DatabaseManager::DeleteBook(int book_id) {
    if (!db_) return false;

    const char* sql = "DELETE FROM books WHERE id = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        DebugLogger::log("Failed to prepare statement for DeleteBook: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    sqlite3_bind_int(stmt, 1, book_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        DebugLogger::log("Failed to execute DeleteBook: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    DebugLogger::log("Deleted book with id: " + std::to_string(book_id));
    return true;
}

bool DatabaseManager::GetBookPath(int book_id, std::string& path) {
    if (!db_) return false;

    const char* sql = "SELECT path FROM books WHERE id = ?;";
    sqlite3_stmt* stmt;
    bool found = false;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, book_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            found = true;
        }
    } else {
        DebugLogger::log("Failed to prepare statement for GetBookPath: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_finalize(stmt);
    return found;
}
