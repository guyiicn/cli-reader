#pragma once

#include <string>
#include <vector>
#include <map>
#include "GoogleAuthManager.h"
#include "Book.h" // Include Book struct for method signatures

// A simple struct to hold file metadata
struct DriveFile {
    std::string id;
    std::string name;
    std::string modified_time; // ISO 8601 format
    std::string md5_checksum;
    std::map<std::string, std::string> app_properties;
};

class GoogleDriveManager {
public:
    // Constructor takes a reference to an auth manager.
    explicit GoogleDriveManager(GoogleAuthManager& auth_manager);

    // Finds the app's root folder, creating it if it doesn't exist.
    // Returns the folder ID.
    std::string find_or_create_app_folder();

    // Lists all files within a given folder ID.
    // If last_sync_timestamp is provided (ISO 8601 format), it will only fetch files modified after that time.
    std::vector<DriveFile> list_files_in_folder(const std::string& folder_id, const std::string& last_sync_timestamp = "");

    // Uploads a local file with all its metadata.
    std::string upload_file(const Book& book, const std::string& folder_id);

    // Downloads a file from Drive to a local path.
    bool download_file(const std::string& file_id, const std::string& save_path);
    
    // Deletes a file on Google Drive.
    bool delete_file(const std::string& file_id);

    // Retrieves metadata for a single file.
    DriveFile get_file_metadata(const std::string& file_id);

    // Updates custom metadata (appProperties) for a file from a Book object.
    bool update_file_metadata(const Book& book);

    // Deletes all files within the app folder. For testing purposes.
    void delete_all_files_in_app_folder();

private:
    // --- Member Variables ---
    GoogleAuthManager& auth_manager_;
    std::string app_folder_id_;
};
