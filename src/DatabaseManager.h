#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <string>
#include <vector>
#include <optional>
#include <map>
#include "Book.h"

struct sqlite3; // Forward declaration

class DatabaseManager {
public:
    explicit DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    bool InitDatabase();
    bool AddBook(const Book& book);
    bool BookExists(const std::string& hash);
    std::vector<Book> GetAllBooks();
    std::optional<Book> GetBookByUUID(const std::string& uuid);
    std::optional<Book> GetBookByHash(const std::string& hash);
    bool UpdateProgress(const std::string& book_uuid, int current_page);
    bool UpdateProgressAndTimestamp(const std::string& uuid, int current_page, time_t last_read_time);
    bool UpdateLastReadTime(const std::string& uuid);
    bool DeleteBook(const std::string& book_uuid);
    bool UpdateOcrStatus(const std::string& book_uuid, const std::string& status);
    bool UpdatePdfHealthStatus(const std::string& book_uuid, const std::string& health_status, const std::string& content_type);

    // Cloud Sync Specific
    bool UpdateBookSyncStatus(const std::string& uuid, const std::string& sync_status, const std::string& google_drive_file_id);
    bool UpdateBookToCloudOnly(const std::string& uuid);
    bool UpdateBookToLocalOnly(const std::string& uuid);
    bool UpdateBookFields(const std::string& uuid, const std::string& new_path, const std::string& new_hash);

    std::string GetDatabasePath() const;
    void InitializeSystemSettings(const std::string& base_path);
    std::map<std::string, std::string> GetAllSettings() const;
    bool SetSetting(const std::string& key, const std::string& value);

    // --- Multi-Device Sync Methods ---
    std::map<std::string, Book> GetAllBooksByDriveId() const;
    void AddOrUpdateBookFromCloud(const Book& cloud_book);

private:
    void UpgradeSchema();
    std::string db_path_;
    sqlite3* db_ = nullptr;
};

#endif // DATABASE_MANAGER_H