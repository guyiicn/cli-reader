#include "GoogleDriveManager.h"
#include <cpr/cpr.h>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

GoogleDriveManager::GoogleDriveManager(GoogleAuthManager& auth_manager) : auth_manager_(auth_manager) {}

std::string GoogleDriveManager::find_or_create_app_folder() {
    if (!app_folder_id_.empty()) {
        return app_folder_id_;
    }

    const std::string folder_name = "EbookReaderSync";
    std::cout << "\nSearching for app folder '" << folder_name << "'..." << std::endl;

    bool needs_interaction; // Dummy variable, not used here
    std::string access_token = auth_manager_.get_access_token(needs_interaction);
    if (access_token.empty()) {
        std::cerr << "Could not get access token." << std::endl;
        return "";
    }

    cpr::Response search_response = cpr::Get(
        cpr::Url{"https://www.googleapis.com/drive/v3/files"},
        cpr::Bearer{access_token},
        cpr::Parameters{
            {"q", "mimeType='application/vnd.google-apps.folder' and name='" + folder_name + "' and trashed=false"},
            {"spaces", "drive"},
            {"fields", "files(id, name)"}
        }
    );

    if (search_response.status_code == 200) {
        nlohmann::json search_data = nlohmann::json::parse(search_response.text);
        if (!search_data["files"].empty()) {
            app_folder_id_ = search_data["files"][0]["id"];
            std::cout << "Found folder. ID: " << app_folder_id_ << std::endl;
            return app_folder_id_;
        }
    } else {
        std::cerr << "Error searching for folder: " << search_response.text << std::endl;
        return "";
    }

    std::cout << "Folder not found. Creating it..." << std::endl;
    nlohmann::json create_metadata;
    create_metadata["name"] = folder_name;
    create_metadata["mimeType"] = "application/vnd.google-apps.folder";

    cpr::Response create_response = cpr::Post(
        cpr::Url{"https://www.googleapis.com/drive/v3/files"},
        cpr::Bearer{access_token},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Body{create_metadata.dump()}
    );

    if (create_response.status_code == 200) {
        nlohmann::json create_data = nlohmann::json::parse(create_response.text);
        app_folder_id_ = create_data["id"];
        std::cout << "Successfully created folder. ID: " << app_folder_id_ << std::endl;
        return app_folder_id_;
    }
    
    std::cerr << "Error creating folder: " << create_response.text << std::endl;
    return "";
}

std::vector<DriveFile> GoogleDriveManager::list_files_in_folder(const std::string& folder_id, const std::string& last_sync_timestamp) {
    std::vector<DriveFile> files;
    bool needs_interaction; // Dummy variable, not used here
    std::string access_token = auth_manager_.get_access_token(needs_interaction);
    if (access_token.empty()) {
        std::cerr << "Could not get access token." << std::endl;
        return files;
    }

    std::string query = "'" + folder_id + "' in parents and trashed=false";
    if (!last_sync_timestamp.empty()) {
        query += " and modifiedTime > '" + last_sync_timestamp + "'";
    }

    cpr::Response r = cpr::Get(
        cpr::Url{"https://www.googleapis.com/drive/v3/files"},
        cpr::Bearer{access_token},
        cpr::Parameters{
            {"q", query},
            {"fields", "files(id, name, modifiedTime, md5Checksum, appProperties)"}
        }
    );

    if (r.status_code != 200) {
        std::cerr << "Error listing files: " << r.text << std::endl;
        return files;
    }

    try {
        nlohmann::json list_data = nlohmann::json::parse(r.text);
        for (const auto& item : list_data["files"]) {
            DriveFile file;
            file.id = item.value("id", "");
            file.name = item.value("name", "");
            file.modified_time = item.value("modifiedTime", "");
            file.md5_checksum = item.value("md5Checksum", "");
            if (item.contains("appProperties")) {
                for (auto const& [key, val] : item["appProperties"].items()) {
                    file.app_properties[key] = val;
                }
            }
            files.push_back(file);
        }
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "Error parsing file list JSON: " << e.what() << std::endl;
    }

    return files;
}

std::string GoogleDriveManager::upload_file(const Book& book, const std::string& folder_id) {
    bool needs_interaction; // Dummy variable, not used here
    std::string access_token = auth_manager_.get_access_token(needs_interaction);
    if (access_token.empty()) {
        std::cerr << "Could not get access token for upload." << std::endl;
        return "";
    }

    nlohmann::json metadata;
    metadata["name"] = fs::path(book.path).filename().string();
    metadata["parents"] = {folder_id};
    metadata["appProperties"] = {
        {"uuid", book.uuid},
        {"title", book.title},
        {"author", book.author},
        {"hash", book.hash},
        {"currentPage", std::to_string(book.current_page)},
        {"totalPages", std::to_string(book.total_pages)},
        {"lastReadTime", std::to_string(book.last_read_time)}
    };

    cpr::Response init_response = cpr::Post(
        cpr::Url{"https://www.googleapis.com/upload/drive/v3/files?uploadType=resumable"},
        cpr::Bearer{access_token},
        cpr::Header{{"Content-Type", "application/json; charset=UTF-8"}},
        cpr::Body{metadata.dump()}
    );

    if (init_response.status_code != 200) {
        std::cerr << "Error initiating resumable upload: " << init_response.text << std::endl;
        return "";
    }

    std::string session_uri = init_response.header["Location"];
    if (session_uri.empty()) {
        std::cerr << "Error: Did not receive a session URI for resumable upload." << std::endl;
        return "";
    }
    
    std::ifstream file_stream(book.path, std::ios::binary);
    if (!file_stream) {
        std::cerr << "Error: Could not open file for upload: " << book.path << std::endl;
        return "";
    }
    std::string file_content((std::istreambuf_iterator<char>(file_stream)), std::istreambuf_iterator<char>());

    cpr::Response upload_response = cpr::Put(
        cpr::Url{session_uri},
        cpr::Body{file_content},
        cpr::Header{{"Content-Type", "application/octet-stream"}}
    );

    if (upload_response.status_code == 200) {
        nlohmann::json final_data = nlohmann::json::parse(upload_response.text);
        return final_data.value("id", "");
    }
    
    std::cerr << "Error uploading file content: " << upload_response.text << std::endl;
    return "";
}

bool GoogleDriveManager::download_file(const std::string& file_id, const std::string& save_path) {
    bool needs_interaction; // Dummy variable, not used here
    std::string access_token = auth_manager_.get_access_token(needs_interaction);
    if (access_token.empty()) {
        std::cerr << "Could not get access token for download." << std::endl;
        return false;
    }

    std::ofstream out_file(save_path, std::ios::binary);
    if (!out_file.is_open()) {
        std::cerr << "Error: Could not open file for writing: " << save_path << std::endl;
        return false;
    }

    cpr::Response r = cpr::Download(
        out_file,
        cpr::Url{"https://www.googleapis.com/drive/v3/files/" + file_id + "?alt=media"},
        cpr::Bearer{access_token}
    );

    if (r.status_code == 200) {
        std::cout << "File downloaded successfully to " << save_path << std::endl;
        return true;
    }
    
    std::cerr << "Error downloading file: " << r.error.message << std::endl;
    fs::remove(save_path);
    return false;
}

bool GoogleDriveManager::delete_file(const std::string& file_id) {
    bool needs_interaction; // Dummy variable, not used here
    std::string access_token = auth_manager_.get_access_token(needs_interaction);
    if (access_token.empty()) {
        std::cerr << "Could not get access token for delete." << std::endl;
        return false;
    }

    cpr::Response r = cpr::Delete(
        cpr::Url{"https://www.googleapis.com/drive/v3/files/" + file_id},
        cpr::Bearer{access_token}
    );

    if (r.status_code == 204) {
        std::cout << "File with ID: " << file_id << " deleted successfully." << std::endl;
        return true;
    }
    
    std::cerr << "Error deleting file: " << r.text << std::endl;
    return false;
}

DriveFile GoogleDriveManager::get_file_metadata(const std::string& file_id) {
    DriveFile file;
    bool needs_interaction; // Dummy variable, not used here
    std::string access_token = auth_manager_.get_access_token(needs_interaction);
    if (access_token.empty()) {
        std::cerr << "Could not get access token." << std::endl;
        return file;
    }

    cpr::Response r = cpr::Get(
        cpr::Url{"https://www.googleapis.com/drive/v3/files/" + file_id},
        cpr::Bearer{access_token},
        cpr::Parameters{{"fields", "id, name, modifiedTime, md5Checksum, appProperties"}}
    );

    if (r.status_code != 200) {
        std::cerr << "Error getting file metadata: " << r.text << std::endl;
        return file;
    }

    try {
        nlohmann::json item = nlohmann::json::parse(r.text);
        file.id = item.value("id", "");
        file.name = item.value("name", "");
        file.modified_time = item.value("modifiedTime", "");
        file.md5_checksum = item.value("md5Checksum", "");
        if (item.contains("appProperties")) {
            for (auto const& [key, val] : item["appProperties"].items()) {
                file.app_properties[key] = val;
            }
        }
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "Error parsing file metadata JSON: " << e.what() << std::endl;
    }

    return file;
}

bool GoogleDriveManager::update_file_metadata(const Book& book) {
    if (book.google_drive_file_id.empty()) {
        return false;
    }
    bool needs_interaction;
    std::string access_token = auth_manager_.get_access_token(needs_interaction);
    if (access_token.empty()) {
        std::cerr << "Could not get access token for metadata update." << std::endl;
        return false;
    }

    nlohmann::json metadata_patch;
    metadata_patch["appProperties"] = {
        {"uuid", book.uuid},
        {"title", book.title},
        {"author", book.author},
        {"hash", book.hash},
        {"currentPage", std::to_string(book.current_page)},
        {"totalPages", std::to_string(book.total_pages)},
        {"lastReadTime", std::to_string(book.last_read_time)}
    };

    cpr::Response r = cpr::Patch(
        cpr::Url{"https://www.googleapis.com/drive/v3/files/" + book.google_drive_file_id},
        cpr::Bearer{access_token},
        cpr::Header{{"Content-Type", "application/json"}},
        cpr::Body{metadata_patch.dump()}
    );

    if (r.status_code == 200) {
        return true;
    }
    
    std::cerr << "Error updating file metadata: " << r.text << std::endl;
    return false;
}

void GoogleDriveManager::delete_all_files_in_app_folder() {
    std::string folder_id = find_or_create_app_folder();
    if (folder_id.empty()) {
        std::cerr << "Could not get app folder to clear files." << std::endl;
        return;
    }
    std::cout << "Clearing all files in app folder..." << std::endl;
    std::vector<DriveFile> files = list_files_in_folder(folder_id);
    for (const auto& file : files) {
        delete_file(file.id);
    }
    std::cout << "App folder cleared." << std::endl;
}