#ifndef SYNC_CONTROLLER_H
#define SYNC_CONTROLLER_H

#include <vector>
#include <string>
#include <functional>
#include "Book.h"
#include "DatabaseManager.h"
#include "GoogleDriveManager.h"
#include "ConfigManager.h"
#include "nlohmann/json.hpp"
#include "CommonTypes.h"

class SyncController {
public:
    SyncController(DatabaseManager& db_manager, GoogleDriveManager& drive_manager, ConfigManager& config_manager);

    void full_sync(std::function<void(bool, std::string)> callback);
    void get_latest_progress_async(const std::string& book_uuid, std::function<void(Book, bool)> callback);
    void sync_progress_before_local_delete(const std::string& book_uuid);
    void upload_progress_async(const Book& book, std::function<void(bool)> callback);
    
    void upload_book(const std::string& book_uuid, std::function<void(bool, std::string)> callback);
    void download_book(const Book& book, const std::string& dest_folder, std::function<void(bool, std::string)> callback);
    void verify_and_download_book_async(const Book& book, const std::string& dest_folder, std::function<void(bool, std::string)> callback);

    void delete_cloud_file_async(const std::string& book_uuid, std::function<void(bool)> callback);

private:
    DatabaseManager& db_manager_;
    GoogleDriveManager& drive_manager_;
    ConfigManager& config_manager_;
};

#endif // SYNC_CONTROLLER_H
