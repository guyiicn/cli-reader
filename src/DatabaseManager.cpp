#include "DatabaseManager.h"
#include "DebugLogger.h"
#include "SystemUtils.h"
#include "sqlite3.h"
#include "uuid.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <filesystem>

DatabaseManager::DatabaseManager(const std::string& db_path) : db_path_(db_path) {
    if (sqlite3_open(db_path.c_str(), &db_)) {
        DebugLogger::log("FATAL: Can't open database: " + std::string(sqlite3_errmsg(db_)));
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

void DatabaseManager::UpgradeSchema() {
    if (!db_) return;
    DebugLogger::log("Checking database schema...");

    const char* sql_check_uuid = "PRAGMA table_info(books);";
    sqlite3_stmt* stmt;
    bool uuid_exists = false;
    bool sync_status_exists = false;
    bool gdrive_id_exists = false;
    bool format_exists = false;
    bool cover_image_exists = false;

    if (sqlite3_prepare_v2(db_, sql_check_uuid, -1, &stmt, 0) != SQLITE_OK) {
        DebugLogger::log("Failed to prepare statement for schema check.");
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string column_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (column_name == "uuid") uuid_exists = true;
        if (column_name == "sync_status") sync_status_exists = true;
        if (column_name == "google_drive_file_id") gdrive_id_exists = true;
        if (column_name == "format") format_exists = true;
        if (column_name == "cover_image_path") cover_image_exists = true;
    }
    sqlite3_finalize(stmt);

    char* err_msg = nullptr;
    auto execute_sql = [&](const std::string& sql, const std::string& msg) {
        if (sqlite3_exec(db_, sql.c_str(), 0, 0, &err_msg) != SQLITE_OK) {
            DebugLogger::log(msg + ": " + err_msg);
            sqlite3_free(err_msg);
        }
    };

    if (!uuid_exists) {
        DebugLogger::log("Upgrading schema: adding 'uuid' column.");
        execute_sql("ALTER TABLE books ADD COLUMN uuid TEXT;", "Failed to add 'uuid'");
    }
    if (!sync_status_exists) {
        DebugLogger::log("Upgrading schema: adding 'sync_status' column.");
        execute_sql("ALTER TABLE books ADD COLUMN sync_status TEXT DEFAULT 'local';", "Failed to add 'sync_status'");
    }
    if (!gdrive_id_exists) {
        DebugLogger::log("Upgrading schema: adding 'google_drive_file_id' column.");
        execute_sql("ALTER TABLE books ADD COLUMN google_drive_file_id TEXT;", "Failed to add 'google_drive_file_id'");
    }
    if (!format_exists) {
        DebugLogger::log("Upgrading schema: adding 'format' column.");
        execute_sql("ALTER TABLE books ADD COLUMN format TEXT;", "Failed to add 'format'");
    }
    if (!cover_image_exists) {
        DebugLogger::log("Upgrading schema: adding 'cover_image_path' column.");
        execute_sql("ALTER TABLE books ADD COLUMN cover_image_path TEXT;", "Failed to add 'cover_image_path'");
    }
}


bool DatabaseManager::InitDatabase() {
    if (!db_) return false;

    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS books (
            uuid TEXT PRIMARY KEY NOT NULL,
            title TEXT NOT NULL,
            author TEXT,
            path TEXT,
            hash TEXT,
            cover_image_path TEXT,
            add_date INTEGER,
            last_read_time INTEGER,
            current_page INTEGER DEFAULT 0,
            total_pages INTEGER DEFAULT 0,
            pdf_content_type TEXT,
            pdf_health_status TEXT,
            ocr_status TEXT DEFAULT 'none',
            sync_status TEXT DEFAULT 'local',
            google_drive_file_id TEXT,
            format TEXT
        );
    )";
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, sql, 0, 0, &err_msg) != SQLITE_OK) {
        DebugLogger::log("Failed to create table: " + std::string(err_msg));
        sqlite3_free(err_msg);
        if (std::string(err_msg).find("table books already exists") == std::string::npos) {
            return false;
        }
    }
    
    UpgradeSchema();

    char* err_msg2 = nullptr;
    if (sqlite3_exec(db_, "CREATE UNIQUE INDEX IF NOT EXISTS idx_books_hash ON books(hash) WHERE hash IS NOT NULL AND hash != '';", 0, 0, &err_msg2) != SQLITE_OK) {
        DebugLogger::log("Failed to create hash index: " + std::string(err_msg2));
        sqlite3_free(err_msg2);
    }
    if (sqlite3_exec(db_, "CREATE UNIQUE INDEX IF NOT EXISTS idx_books_path ON books(path) WHERE path IS NOT NULL AND path != '';", 0, 0, &err_msg2) != SQLITE_OK) {
        DebugLogger::log("Failed to create path index: " + std::string(err_msg2));
        sqlite3_free(err_msg2);
    }

    DebugLogger::log("Database initialized or upgraded successfully.");
    return true;
}

bool DatabaseManager::AddBook(const Book& book) {
    if (!db_ || book.uuid.empty()) return false;

    const char* sql = R"(
        INSERT OR REPLACE INTO books (
            uuid, title, author, path, hash, cover_image_path, add_date, last_read_time,
            current_page, total_pages, pdf_content_type, pdf_health_status, ocr_status,
            sync_status, google_drive_file_id, format
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        DebugLogger::log("AddBook: Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    sqlite3_bind_text(stmt, 1, book.uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, book.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, book.author.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, book.path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, book.hash.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, book.cover_image_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 7, book.add_date);
    sqlite3_bind_int64(stmt, 8, book.last_read_time);
    sqlite3_bind_int(stmt, 9, book.current_page);
    sqlite3_bind_int(stmt, 10, book.total_pages);
    sqlite3_bind_text(stmt, 11, book.pdf_content_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, book.pdf_health_status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, book.ocr_status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 14, book.sync_status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 15, book.google_drive_file_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 16, book.format.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        DebugLogger::log("AddBook: Failed to execute statement: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    DebugLogger::log("Successfully added/replaced book: " + book.title);
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
        DebugLogger::log("BookExists: Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
    }
    sqlite3_finalize(stmt);
    return exists;
}

std::vector<Book> DatabaseManager::GetAllBooks() {
    std::vector<Book> books;
    if (!db_) return books;

    const char* sql = "SELECT uuid, title, author, path, hash, current_page, total_pages, last_read_time, add_date, cover_image_path, format, pdf_content_type, pdf_health_status, ocr_status, sync_status, google_drive_file_id FROM books ORDER BY last_read_time DESC;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        DebugLogger::log("GetAllBooks: Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return books;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Book book;
        book.uuid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        book.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        book.author = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        book.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        book.hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        book.current_page = sqlite3_column_int(stmt, 5);
        book.total_pages = sqlite3_column_int(stmt, 6);
        book.last_read_time = sqlite3_column_int64(stmt, 7);
        book.add_date = sqlite3_column_int64(stmt, 8);
        const char* cover = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        book.cover_image_path = cover ? cover : "";
        const char* format = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        book.format = format ? format : "";
        const char* content_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        book.pdf_content_type = content_type ? content_type : "unknown";
        const char* health_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        book.pdf_health_status = health_status ? health_status : "unchecked";
        const char* ocr_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
        book.ocr_status = ocr_status ? ocr_status : "none";
        const char* sync_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
        book.sync_status = sync_status ? sync_status : "local";
        const char* gdrive_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15));
        book.google_drive_file_id = gdrive_id ? gdrive_id : "";
        books.push_back(book);
    }

    sqlite3_finalize(stmt);
    return books;
}

std::optional<Book> DatabaseManager::GetBookByUUID(const std::string& uuid) {
    if (!db_) return std::nullopt;
    const char* sql = "SELECT uuid, title, author, path, hash, current_page, total_pages, last_read_time, add_date, cover_image_path, format, pdf_content_type, pdf_health_status, ocr_status, sync_status, google_drive_file_id FROM books WHERE uuid = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        DebugLogger::log("GetBookByUUID: Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Book book;
        book.uuid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        book.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        book.author = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        book.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        book.hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        book.current_page = sqlite3_column_int(stmt, 5);
        book.total_pages = sqlite3_column_int(stmt, 6);
        book.last_read_time = sqlite3_column_int64(stmt, 7);
        book.add_date = sqlite3_column_int64(stmt, 8);
        const char* cover = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        book.cover_image_path = cover ? cover : "";
        const char* format = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        book.format = format ? format : "";
        const char* content_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        book.pdf_content_type = content_type ? content_type : "unknown";
        const char* health_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        book.pdf_health_status = health_status ? health_status : "unchecked";
        const char* ocr_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
        book.ocr_status = ocr_status ? ocr_status : "none";
        const char* sync_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
        book.sync_status = sync_status ? sync_status : "local";
        const char* gdrive_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15));
        book.google_drive_file_id = gdrive_id ? gdrive_id : "";
        sqlite3_finalize(stmt);
        return book;
    }
    
    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::optional<Book> DatabaseManager::GetBookByHash(const std::string& hash) {
    if (!db_) return std::nullopt;
    const char* sql = "SELECT uuid, title, author, path, hash, current_page, total_pages, last_read_time, add_date, cover_image_path, format, pdf_content_type, pdf_health_status, ocr_status, sync_status, google_drive_file_id FROM books WHERE hash = ?;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        DebugLogger::log("GetBookByHash: Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Book book;
        book.uuid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        book.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        book.author = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        book.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        book.hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        book.current_page = sqlite3_column_int(stmt, 5);
        book.total_pages = sqlite3_column_int(stmt, 6);
        book.last_read_time = sqlite3_column_int64(stmt, 7);
        book.add_date = sqlite3_column_int64(stmt, 8);
        const char* cover = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        book.cover_image_path = cover ? cover : "";
        const char* format = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        book.format = format ? format : "";
        const char* content_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        book.pdf_content_type = content_type ? content_type : "unknown";
        const char* health_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        book.pdf_health_status = health_status ? health_status : "unchecked";
        const char* ocr_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
        book.ocr_status = ocr_status ? ocr_status : "none";
        const char* sync_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
        book.sync_status = sync_status ? sync_status : "local";
        const char* gdrive_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15));
        book.google_drive_file_id = gdrive_id ? gdrive_id : "";
        sqlite3_finalize(stmt);
        return book;
    }
    
    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool DatabaseManager::UpdateProgress(const std::string& book_uuid, int current_page) {
    if (!db_) return false;
    const char* sql = "UPDATE books SET current_page = ? WHERE uuid = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, current_page);
    sqlite3_bind_text(stmt, 2, book_uuid.c_str(), -1, SQLITE_STATIC);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::UpdateProgressAndTimestamp(const std::string& book_uuid, int current_page, time_t last_read_time) {
    if (!db_) return false;
    const char* sql = "UPDATE books SET current_page = ?, last_read_time = ? WHERE uuid = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        DebugLogger::log("UpdateProgressAndTimestamp: Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    sqlite3_bind_int(stmt, 1, current_page);
    sqlite3_bind_int64(stmt, 2, last_read_time);
    sqlite3_bind_text(stmt, 3, book_uuid.c_str(), -1, SQLITE_STATIC);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!success) {
        DebugLogger::log("UpdateProgressAndTimestamp: Failed to execute statement: " + std::string(sqlite3_errmsg(db_)));
    }
    
    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::UpdateLastReadTime(const std::string& book_uuid) {
    if (!db_) return false;
    const char* sql = "UPDATE books SET last_read_time = ? WHERE uuid = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    sqlite3_bind_text(stmt, 2, book_uuid.c_str(), -1, SQLITE_STATIC);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::DeleteBook(const std::string& book_uuid) {
    if (!db_) return false;
    const char* sql = "DELETE FROM books WHERE uuid = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, book_uuid.c_str(), -1, SQLITE_STATIC);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::UpdateOcrStatus(const std::string& book_uuid, const std::string& status) {
    if (!db_) return false;
    const char* sql = "UPDATE books SET ocr_status = ? WHERE uuid = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, book_uuid.c_str(), -1, SQLITE_STATIC);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::UpdatePdfHealthStatus(const std::string& book_uuid, const std::string& health_status, const std::string& content_type) {
    if (!db_) return false;
    const char* sql = "UPDATE books SET pdf_health_status = ?, pdf_content_type = ? WHERE uuid = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, health_status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, content_type.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, book_uuid.c_str(), -1, SQLITE_STATIC);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

// --- Cloud Sync Specific ---
bool DatabaseManager::UpdateBookSyncStatus(const std::string& book_uuid, const std::string& sync_status, const std::string& google_drive_file_id) {
    if (!db_) return false;
    const char* sql = "UPDATE books SET sync_status = ?, google_drive_file_id = ? WHERE uuid = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, sync_status.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, google_drive_file_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, book_uuid.c_str(), -1, SQLITE_STATIC);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::UpdateBookFields(const std::string& book_uuid, const std::string& new_path, const std::string& new_hash) {
    if (!db_) return false;
    const char* sql = "UPDATE books SET path = ?, hash = ?, sync_status = 'synced' WHERE uuid = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, new_path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, new_hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, book_uuid.c_str(), -1, SQLITE_STATIC);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::UpdateBookToCloudOnly(const std::string& book_uuid) {
    if (!db_) return false;
    const char* sql = "UPDATE books SET path = '', hash = '', sync_status = 'cloud' WHERE uuid = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, book_uuid.c_str(), -1, SQLITE_STATIC);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

bool DatabaseManager::UpdateBookToLocalOnly(const std::string& book_uuid) {
    if (!db_) return false;
    const char* sql = "UPDATE books SET google_drive_file_id = '', sync_status = 'local' WHERE uuid = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, book_uuid.c_str(), -1, SQLITE_STATIC);
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return success;
}

std::string DatabaseManager::GetDatabasePath() const {
    return db_path_;
}

void DatabaseManager::InitializeSystemSettings(const std::string& base_path) {
    if (!db_) return;

    // 1. Create systemInfo table
    const char* create_sql = R"(
        CREATE TABLE IF NOT EXISTS systemInfo (
            key TEXT PRIMARY KEY NOT NULL,
            value TEXT NOT NULL
        );
    )";
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, create_sql, 0, 0, &err_msg) != SQLITE_OK) {
        DebugLogger::log("Failed to create systemInfo table: " + std::string(err_msg));
        sqlite3_free(err_msg);
        return;
    }

    // 2. Check if table is empty (first time setup)
    const char* check_sql = "SELECT COUNT(*) FROM systemInfo;";
    sqlite3_stmt* stmt;
    int count = 0;
    if (sqlite3_prepare_v2(db_, check_sql, -1, &stmt, 0) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);

    // 3. If empty, populate with default values
    if (count == 0) {
        DebugLogger::log("Populating systemInfo table with default settings...");
        
        const char* insert_sql = "INSERT INTO systemInfo (key, value) VALUES (?, ?);";
        
        auto insert_setting = [&](const std::string& key, const std::string& value) {
            sqlite3_stmt* insert_stmt;
            if (sqlite3_prepare_v2(db_, insert_sql, -1, &insert_stmt, 0) == SQLITE_OK) {
                sqlite3_bind_text(insert_stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(insert_stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(insert_stmt) != SQLITE_DONE) {
                    DebugLogger::log("Failed to insert setting '" + key + "': " + sqlite3_errmsg(db_));
                }
                sqlite3_finalize(insert_stmt);
            }
        };

        std::filesystem::path bp(base_path);
        insert_setting("default_path", base_path);
        insert_setting("library_path", (bp / "books").string());
        insert_setting("database_path", (bp / "config").string());
        insert_setting("client_id", "");
        insert_setting("client_secret", "");
        insert_setting("refresh_token", "");
        insert_setting("last_picker_path", SystemUtils::GetHomePath());
    }
}

std::map<std::string, std::string> DatabaseManager::GetAllSettings() const {
    std::map<std::string, std::string> settings;
    if (!db_) return settings;

    const char* sql = "SELECT key, value FROM systemInfo;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        DebugLogger::log("GetAllSettings: Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return settings;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        settings[key] = value;
    }

    sqlite3_finalize(stmt);
    return settings;
}

bool DatabaseManager::SetSetting(const std::string& key, const std::string& value) {
    if (!db_) return false;
    const char* sql = "INSERT OR REPLACE INTO systemInfo (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        DebugLogger::log("SetSetting: Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    
    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!success) {
        DebugLogger::log("SetSetting: Failed to execute statement for key '" + key + "': " + std::string(sqlite3_errmsg(db_)));
    }
    
    sqlite3_finalize(stmt);
    return success;
}


// --- Multi-Device Sync Methods ---

std::map<std::string, Book> DatabaseManager::GetAllBooksByDriveId() const {
    std::map<std::string, Book> books;
    if (!db_) return books;

    const char* sql = "SELECT uuid, title, author, path, hash, current_page, total_pages, last_read_time, add_date, cover_image_path, format, pdf_content_type, pdf_health_status, ocr_status, sync_status, google_drive_file_id FROM books WHERE google_drive_file_id IS NOT NULL AND google_drive_file_id != '' ORDER BY last_read_time DESC;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        DebugLogger::log("GetAllBooksByDriveId: Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        return books;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Book book;
        book.uuid = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        book.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        book.author = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        book.path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        book.hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        book.current_page = sqlite3_column_int(stmt, 5);
        book.total_pages = sqlite3_column_int(stmt, 6);
        book.last_read_time = sqlite3_column_int64(stmt, 7);
        book.add_date = sqlite3_column_int64(stmt, 8);
        const char* cover = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
        book.cover_image_path = cover ? cover : "";
        const char* format = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        book.format = format ? format : "";
        const char* content_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        book.pdf_content_type = content_type ? content_type : "unknown";
        const char* health_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        book.pdf_health_status = health_status ? health_status : "unchecked";
        const char* ocr_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
        book.ocr_status = ocr_status ? ocr_status : "none";
        const char* sync_status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
        book.sync_status = sync_status ? sync_status : "local";
        const char* gdrive_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15));
        book.google_drive_file_id = gdrive_id ? gdrive_id : "";
        
        if (!book.google_drive_file_id.empty()) {
            books[book.google_drive_file_id] = book;
        }
    }

    sqlite3_finalize(stmt);
    return books;
}

void DatabaseManager::AddOrUpdateBookFromCloud(const Book& cloud_book) {
    if (!db_ || cloud_book.google_drive_file_id.empty()) {
        return;
    }

    // Check if a book with this google_drive_file_id already exists
    const char* check_sql = "SELECT uuid, last_read_time FROM books WHERE google_drive_file_id = ?;";
    sqlite3_stmt* check_stmt;
    std::string existing_uuid;
    time_t existing_last_read_time = 0;

    if (sqlite3_prepare_v2(db_, check_sql, -1, &check_stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(check_stmt, 1, cloud_book.google_drive_file_id.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(check_stmt) == SQLITE_ROW) {
            existing_uuid = reinterpret_cast<const char*>(sqlite3_column_text(check_stmt, 0));
            existing_last_read_time = sqlite3_column_int64(check_stmt, 1);
        }
    }
    sqlite3_finalize(check_stmt);

    if (!existing_uuid.empty()) {
        // Book exists, update it only if the cloud version is newer
        if (cloud_book.last_read_time > existing_last_read_time) {
            const char* update_sql = "UPDATE books SET current_page = ?, last_read_time = ? WHERE uuid = ?;";
            sqlite3_stmt* update_stmt;
            if (sqlite3_prepare_v2(db_, update_sql, -1, &update_stmt, 0) == SQLITE_OK) {
                sqlite3_bind_int(update_stmt, 1, cloud_book.current_page);
                sqlite3_bind_int64(update_stmt, 2, cloud_book.last_read_time);
                sqlite3_bind_text(update_stmt, 3, existing_uuid.c_str(), -1, SQLITE_STATIC);
                if (sqlite3_step(update_stmt) != SQLITE_DONE) {
                    DebugLogger::log("AddOrUpdateBookFromCloud: Failed to update progress for UUID " + existing_uuid);
                }
                sqlite3_finalize(update_stmt);
            }
        }
    } else {
        // Book does not exist, insert a new record
        Book new_book = cloud_book;
        new_book.uuid = uuid::generate_uuid_v4(); // Generate a new local UUID
        new_book.sync_status = "cloud";
        new_book.path = ""; // No local path yet
        AddBook(new_book);
    }
}

