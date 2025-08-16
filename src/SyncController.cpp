#include "SyncController.h"
#include "SystemUtils.h"
#include "uuid.h"
#include "DebugLogger.h"
#include <filesystem>
#include <map>
#include <set>
#include <fstream>
#include <thread>

namespace fs = std::filesystem;
using json = nlohmann::json;

SyncController::SyncController(DatabaseManager& db_manager, GoogleDriveManager& drive_manager, ConfigManager& config_manager)
    : db_manager_(db_manager), drive_manager_(drive_manager), config_manager_(config_manager) {}

void SyncController::full_sync(std::function<void(bool, std::string)> callback) {
    DebugLogger::log("Starting full sync...");
    std::string folder_id = drive_manager_.find_or_create_app_folder();
    if (folder_id.empty()) {
        callback(false, "Could not access cloud folder.");
        return;
    }

    // Step 1: Fetch Data
    DebugLogger::log("Fetching remote files and local book map...");
    std::vector<DriveFile> remote_files = drive_manager_.list_files_in_folder(folder_id);
    std::map<std::string, Book> local_books_by_drive_id = db_manager_.GetAllBooksByDriveId();
    std::set<std::string> processed_drive_ids;

    DebugLogger::log("Remote files found: " + std::to_string(remote_files.size()));
    DebugLogger::log("Local books with Drive ID found: " + std::to_string(local_books_by_drive_id.size()));

    // Step 2: Main Reconciliation Loop
    for (const auto& remote_file : remote_files) {
        auto it = local_books_by_drive_id.find(remote_file.id);

        if (it != local_books_by_drive_id.end()) {
            // Case A: Book exists locally and remotely
            Book& local_book = it->second;
            processed_drive_ids.insert(remote_file.id);

            time_t remote_timestamp = 0;
            if (remote_file.app_properties.count("lastReadTime")) {
                try { remote_timestamp = std::stoll(remote_file.app_properties.at("lastReadTime")); } catch(...) {}
            }

            if (remote_timestamp > local_book.last_read_time) {
                DebugLogger::log("Remote is newer for '" + local_book.title + "'. Updating local progress.");
                int remote_page = 0;
                if (remote_file.app_properties.count("currentPage")) {
                    try { remote_page = std::stoi(remote_file.app_properties.at("currentPage")); } catch(...) {}
                }
                db_manager_.UpdateProgressAndTimestamp(local_book.uuid, remote_page, remote_timestamp);
            } else if (local_book.last_read_time > remote_timestamp) {
                DebugLogger::log("Local is newer for '" + local_book.title + "'. Uploading progress.");
                upload_progress_async(local_book, nullptr);
            }

        } else {
            // Case B: Book exists only remotely
            DebugLogger::log("Found new remote file: " + remote_file.name + ". Creating local record.");
            Book new_cloud_book;
            new_cloud_book.google_drive_file_id = remote_file.id;
            new_cloud_book.title = remote_file.app_properties.count("title") ? remote_file.app_properties.at("title") : fs::path(remote_file.name).stem().string();
            new_cloud_book.author = remote_file.app_properties.count("author") ? remote_file.app_properties.at("author") : "Unknown";
            new_cloud_book.hash = remote_file.app_properties.count("hash") ? remote_file.app_properties.at("hash") : "";
            
            // Enhanced logic for format
            if (remote_file.app_properties.count("format")) {
                new_cloud_book.format = remote_file.app_properties.at("format");
            } else {
                new_cloud_book.format = SystemUtils::get_file_extension(remote_file.name);
            }

            try {
                new_cloud_book.current_page = remote_file.app_properties.count("currentPage") ? std::stoi(remote_file.app_properties.at("currentPage")) : 0;
                new_cloud_book.total_pages = remote_file.app_properties.count("totalPages") ? std::stoi(remote_file.app_properties.at("totalPages")) : 0;
                new_cloud_book.last_read_time = remote_file.app_properties.count("lastReadTime") ? std::stoll(remote_file.app_properties.at("lastReadTime")) : 0;
            } catch (...) {
                // Handle potential conversion errors gracefully
                new_cloud_book.current_page = 0;
                new_cloud_book.total_pages = 0;
                new_cloud_book.last_read_time = 0;
            }
            new_cloud_book.add_date = new_cloud_book.last_read_time; // Use last read time as a proxy for add date
            
            db_manager_.AddOrUpdateBookFromCloud(new_cloud_book);
        }
    }

    // Case C (Local only books) is intentionally skipped as per our manual upload design.
    
    DebugLogger::log("Full sync finished.");
    callback(true, "Sync finished.");
}

void SyncController::get_latest_progress_async(const std::string& book_uuid, std::function<void(Book, bool)> callback) {
    std::thread([this, book_uuid, callback] {
        auto book_opt = db_manager_.GetBookByUUID(book_uuid);
        if (!book_opt) {
            callback({}, false);
            return;
        }
        Book local_book = *book_opt;

        DriveFile remote_file = drive_manager_.get_file_metadata(local_book.google_drive_file_id);
        if (remote_file.id.empty()) {
            callback(local_book, true); // No remote file, return local version
            return;
        }

        time_t remote_timestamp = 0;
        int remote_page = 0;
        if (remote_file.app_properties.count("lastReadTime")) {
            try { remote_timestamp = std::stoll(remote_file.app_properties.at("lastReadTime")); } catch(...) {}
        }
        if (remote_file.app_properties.count("currentPage")) {
            try { remote_page = std::stoi(remote_file.app_properties.at("currentPage")); } catch(...) {}
        }

        if (remote_timestamp > local_book.last_read_time) {
            db_manager_.UpdateProgressAndTimestamp(local_book.uuid, remote_page, remote_timestamp);
            local_book.current_page = remote_page;
            local_book.last_read_time = remote_timestamp;
        }
        callback(local_book, true);
    }).detach();
}

void SyncController::sync_progress_before_local_delete(const std::string& book_uuid) {
    // This is a blocking call for simplicity as UI is already in a loading state.
    // A fully async version would require more complex state management.
    auto book_opt = db_manager_.GetBookByUUID(book_uuid);
    if (!book_opt) return;
    Book local_book = *book_opt;

    DriveFile remote_file = drive_manager_.get_file_metadata(local_book.google_drive_file_id);
    if (remote_file.id.empty()) return;

    time_t remote_timestamp = 0;
    int remote_page = 0;
    if (remote_file.app_properties.count("lastReadTime")) {
        try { remote_timestamp = std::stoll(remote_file.app_properties.at("lastReadTime")); } catch(...) {}
    }
    if (remote_file.app_properties.count("currentPage")) {
        try { remote_page = std::stoi(remote_file.app_properties.at("currentPage")); } catch(...) {}
    }

    if (remote_timestamp > local_book.last_read_time) {
        db_manager_.UpdateProgressAndTimestamp(local_book.uuid, remote_page, remote_timestamp);
    }
}

void SyncController::upload_progress_async(const Book& book, std::function<void(bool)> callback) {
    if (book.google_drive_file_id.empty()) {
        if(callback) callback(false);
        return;
    }
    std::thread([this, book, callback] {
        bool success = drive_manager_.update_file_metadata(book);
        if (callback) callback(success);
    }).detach();
}



void SyncController::upload_book(const std::string& book_uuid, std::function<void(bool, std::string)> callback) {
    auto book_opt = db_manager_.GetBookByUUID(book_uuid);
    if (!book_opt) {
        callback(false, "Book not found in local database.");
        return;
    }
    Book book = *book_opt;

    std::string folder_id = drive_manager_.find_or_create_app_folder();
    if (folder_id.empty()) {
        callback(false, "Could not find or create cloud sync folder.");
        return;
    }

    std::string new_id = drive_manager_.upload_file(book, folder_id);
    if (!new_id.empty()) {
        db_manager_.UpdateBookSyncStatus(book.uuid, "synced", new_id);
        callback(true, "Upload successful.");
    } else {
        callback(false, "Upload failed.");
    }
}

void SyncController::download_book(const Book& book, const std::string& dest_folder, std::function<void(bool, std::string)> callback) {
    std::string format_lower = book.format;
    std::transform(format_lower.begin(), format_lower.end(), format_lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    std::string filename = book.title + "." + format_lower;
    fs::path dest_path = fs::path(dest_folder) / filename;
    bool success = drive_manager_.download_file(book.google_drive_file_id, dest_path.string());

    if (success) {
        std::string hash = SystemUtils::CalculateFileHash(dest_path.string());
        db_manager_.UpdateBookFields(book.uuid, dest_path.string(), hash);
        callback(true, "Download successful.");
    } else {
        callback(false, "Download failed.");
    }
}

void SyncController::verify_and_download_book_async(const Book& book, const std::string& dest_folder, std::function<void(bool, std::string)> callback) {
    std::thread([this, book, dest_folder, callback] {
        // 1. Pre-flight check
        DriveFile metadata = drive_manager_.get_file_metadata(book.google_drive_file_id);
        if (metadata.id.empty()) {
            // File does not exist on cloud, it's a stale record
            db_manager_.DeleteBook(book.uuid);
            callback(false, "File no longer exists in the cloud and has been removed.");
            return;
        }

        // 2. Proceed with download
        std::string format_lower = book.format;
        std::transform(format_lower.begin(), format_lower.end(), format_lower.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        std::string filename = book.title + "." + format_lower;
        fs::path dest_path = fs::path(dest_folder) / filename;
        bool success = drive_manager_.download_file(book.google_drive_file_id, dest_path.string());

        if (success) {
            std::string hash = SystemUtils::CalculateFileHash(dest_path.string());
            db_manager_.UpdateBookFields(book.uuid, dest_path.string(), hash);
            callback(true, "Download successful.");
        } else {
            callback(false, "Download failed.");
        }
    }).detach();
}

void SyncController::delete_cloud_file_async(const std::string& book_uuid, std::function<void(bool)> callback) {
    std::thread([this, book_uuid, callback]() {
        auto book_opt = db_manager_.GetBookByUUID(book_uuid);
        if (!book_opt || book_opt->google_drive_file_id.empty()) {
            if (callback) callback(false);
            return;
        }

        bool success = drive_manager_.delete_file(book_opt->google_drive_file_id);

        if (success) {
            db_manager_.UpdateBookToLocalOnly(book_uuid);
        }

        if (callback) callback(success);
    }).detach();
}
