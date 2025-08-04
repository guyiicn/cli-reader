#include "LibraryManager.h"
#include "DebugLogger.h"
#include "sha256.h"
#include "IBookParser.h"
#include "EpubParser.h"
#include "TxtParser.h"
#include "MobiParser.h"
#include "BookViewModel.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

// Helper to create a parser based on file extension
std::unique_ptr<IBookParser> CreateParserForFile(const std::string& path) {
    std::string extension = fs::path(path).extension().string();
    if (extension == ".epub") return std::make_unique<EpubParser>(path);
    if (extension == ".txt") return std::make_unique<TxtParser>(path);
    if (extension == ".mobi" || extension == ".azw3") return std::make_unique<MobiParser>(path);
    return nullptr;
}

LibraryManager::LibraryManager() {
    const char* home_dir = getenv("HOME");
    if (home_dir == nullptr) {
        throw std::runtime_error("Error: HOME environment variable not set.");
    }
    library_path_ = std::string(home_dir) + "/.all_reader";
    EnsureLibraryExists();
}

void LibraryManager::EnsureLibraryExists() const {
    if (!fs::exists(library_path_)) {
        try {
            fs::create_directories(library_path_);
        } catch (const fs::filesystem_error& e) {
            throw std::runtime_error("Error creating library directory: " + std::string(e.what()));
        }
    }
}

std::string LibraryManager::GetLibraryPath() const {
    return library_path_;
}

std::string LibraryManager::AddBook(const std::string& source_path, DatabaseManager& db_manager, int screen_w, int screen_h) {
    if (!fs::exists(source_path)) {
        return "Error: Source file does not exist.";
    }

    // 1. Calculate hash
    std::ifstream file_stream(source_path, std::ios::binary);
    if (!file_stream) {
        return "Error: Could not open source file for hashing.";
    }
    std::string hash;
    picosha2::get_hash_hex_string(file_stream, hash);
    file_stream.close();

    // 2. Check if book exists in DB
    if (db_manager.BookExists(hash)) {
        return "Book already exists in the library.";
    }

    // 3. Copy file to library
    fs::path source_p(source_path);
    fs::path dest_p = fs::path(library_path_) / source_p.filename();
    try {
        fs::copy_file(source_p, dest_p, fs::copy_options::overwrite_existing);
    } catch (const fs::filesystem_error& e) {
        return "Error copying file: " + std::string(e.what());
    }

    // 4. Extract metadata and paginate
    auto parser = CreateParserForFile(dest_p.string());
    if (!parser) {
        fs::remove(dest_p); // Clean up copied file
        return "Error: Unsupported file type.";
    }

    BookViewModel temp_model(CreateParserForFile(dest_p.string()));
    // Use approximate dimensions for pre-calculation. This is a trade-off.
    temp_model.Paginate(screen_w > 0 ? screen_w - 4 : 80, screen_h > 0 ? screen_h - 8 : 24);
    
    Book new_book;
    new_book.title = parser->GetTitle();
    new_book.author = parser->GetAuthor();
    new_book.path = dest_p.string();
    new_book.hash = hash;
    new_book.total_pages = temp_model.GetTotalPages();
    new_book.current_page = 0;

    // 5. Add to database
    if (db_manager.AddBook(new_book)) {
        return "Successfully added: " + new_book.title;
    } else {
        fs::remove(dest_p); // Clean up if DB insert fails
        return "Error: Failed to add book to database.";
    }
}

bool LibraryManager::DeleteBook(int book_id, DatabaseManager& db_manager) {
    std::string path_to_delete;
    if (!db_manager.GetBookPath(book_id, path_to_delete)) {
        DebugLogger::log("Delete failed: Could not find path for book_id " + std::to_string(book_id));
        return false;
    }

    // First, delete the record from the database
    if (!db_manager.DeleteBook(book_id)) {
        DebugLogger::log("Delete failed: Could not remove record from DB for book_id " + std::to_string(book_id));
        return false;
    }

    // Then, delete the file from the filesystem
    if (fs::exists(path_to_delete)) {
        try {
            fs::remove(path_to_delete);
        } catch (const fs::filesystem_error& e) {
            // Log the error, but since the DB record is gone, we still return true.
            // The main goal (removing from library view) is achieved.
            DebugLogger::log("Warning: Failed to delete file " + path_to_delete + ". Error: " + e.what());
        }
    }

    return true;
}
