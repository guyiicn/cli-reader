#include "LibraryManager.h"
#include "DebugLogger.h"
#include "sha256.h"
#include "IBookParser.h"
#include "EpubParser.h"
#include "TxtParser.h"
#include "MobiParser.h"
#include "PdfParser.h"
#include "BookViewModel.h"
#include "SystemUtils.h"
#include "uuid.h" // Required for UUID generation
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <chrono>

namespace fs = std::filesystem;

// Helper to create a parser based on file extension
std::unique_ptr<IBookParser> CreateParserForFile(const std::string& path) {
    std::string extension = fs::path(path).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (extension == ".epub") return std::make_unique<EpubParser>(path);
    if (extension == ".txt") return std::make_unique<TxtParser>(path);
    if (extension == ".mobi" || extension == ".azw3") return std::make_unique<MobiParser>(path);
    if (extension == ".pdf") return std::make_unique<PdfParser>(path);
    return nullptr;
}

LibraryManager::LibraryManager(const ConfigManager& config_manager) 
    : library_path_(config_manager.GetLibraryPath()) 
{
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

void LibraryManager::PerformPdfPreflight(Book& book) {
    DebugLogger::log("Performing pre-flight checks for PDF: " + book.path);
    book.pdf_content_type = "unknown";
    book.pdf_health_status = "healthy"; // Assume healthy unless checks fail

    std::string pdfinfo_cmd = "pdfinfo "" + book.path + """;
    std::string pdfinfo_output = SystemUtils::ExecuteCommand(pdfinfo_cmd);
    int total_pages = 0;

    if (pdfinfo_output.empty() || pdfinfo_output.find("Error") != std::string::npos) {
        book.pdf_health_status = "suspicious";
        DebugLogger::log("PDF marked as suspicious due to pdfinfo failure.");
        return;
    }
    
    size_t pages_pos = pdfinfo_output.find("Pages:");
    if (pages_pos != std::string::npos) {
        try {
            total_pages = std::stoi(pdfinfo_output.substr(pages_pos + 6));
        } catch (...) { /* ignore */ }
    }

    if (total_pages == 0) {
        book.pdf_health_status = "suspicious";
        DebugLogger::log("PDF marked as suspicious due to zero pages found.");
        return;
    }
    book.total_pages = total_pages;

    std::string pdftotext_cmd = "pdftotext -f 1 -l 5 "" + book.path + "" -";
    std::string pdftotext_output = SystemUtils::ExecuteCommand(pdftotext_cmd);
    std::string trimmed_text = pdftotext_output;
    trimmed_text.erase(std::remove_if(trimmed_text.begin(), trimmed_text.end(), ::isspace), trimmed_text.end());

    if (trimmed_text.length() > 20) {
        book.pdf_content_type = "text_based";
        DebugLogger::log("PDF classified as text_based.");
        return;
    }

    std::string pdfimages_cmd = "pdfimages -list "" + book.path + """;
    std::string pdfimages_output = SystemUtils::ExecuteCommand(pdfimages_cmd);
    
    std::istringstream stream(pdfimages_output);
    std::string line;
    int image_count = 0;
    for (int i = 0; std::getline(stream, line); ++i) {
        if (i > 1) image_count++;
    }

    double image_ratio = (total_pages > 0) ? (double)image_count / total_pages : 0.0;
    DebugLogger::log("Image count: " + std::to_string(image_count) + ", Ratio: " + std::to_string(image_ratio));

    if (image_ratio >= 0.9) {
        book.pdf_content_type = "image_based";
        DebugLogger::log("PDF classified as image_based.");
    } else {
        book.pdf_content_type = "text_based";
        book.pdf_health_status = "suspicious";
        DebugLogger::log("PDF classified as text_based (suspicious) due to low image ratio.");
    }
}

std::string LibraryManager::AddBook(const std::string& source_path, DatabaseManager& db_manager, int screen_w, int screen_h) {
    if (!fs::exists(source_path)) {
        return "Error: Source file does not exist.";
    }

    std::string hash = SystemUtils::CalculateFileHash(source_path);
    if (hash.empty()) {
        DebugLogger::log("CRITICAL: Hash generation failed for " + source_path);
        return "Error: Could not calculate file hash.";
    }
    DebugLogger::log("Generated hash for file " + source_path + ": " + hash);

    if (db_manager.BookExists(hash)) {
        DebugLogger::log("Book with hash " + hash + " already exists.");
        return "Book already exists in the library.";
    }

    DebugLogger::log("Book hash " + hash + " is new. Proceeding to add.");

    fs::path source_p(source_path);
    fs::path dest_p = fs::path(library_path_) / source_p.filename();
    try {
        fs::copy_file(source_p, dest_p, fs::copy_options::overwrite_existing);
        DebugLogger::log("Successfully copied file to " + dest_p.string());
    } catch (const fs::filesystem_error& e) {
        DebugLogger::log("ERROR: Failed to copy file: " + std::string(e.what()));
        return "Error copying file: " + std::string(e.what());
    }

    Book new_book;
    new_book.uuid = uuid::generate_uuid_v4();
    new_book.path = dest_p.string();
    new_book.hash = hash;
    new_book.add_date = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    new_book.last_read_time = new_book.add_date;
    
    std::string extension = source_p.extension().string();
    if (!extension.empty()) {
        new_book.format = extension.substr(1);
        std::transform(new_book.format.begin(), new_book.format.end(), new_book.format.begin(), ::toupper);
    }

    if (new_book.format == "PDF") {
        PerformPdfPreflight(new_book);
        new_book.title = source_p.stem().string();
        new_book.author = "Unknown Author";
    } else {
        auto parser = CreateParserForFile(dest_p.string());
        if (!parser) {
            fs::remove(dest_p);
            DebugLogger::log("ERROR: Unsupported file type for: " + dest_p.string());
            return "Error: Unsupported file type.";
        }
        new_book.title = parser->GetTitle();
        new_book.author = parser->GetAuthor();
        
        BookViewModel temp_model(std::move(parser));
        temp_model.Paginate(screen_w > 0 ? screen_w - 4 : 80, screen_h > 0 ? screen_h - 8 : 24);
        new_book.total_pages = temp_model.GetTotalPages();
    }
    
    DebugLogger::log("Attempting to add book to DB: " + new_book.title + " (UUID: " + new_book.uuid + ")");

    if (db_manager.AddBook(new_book)) {
        return "Successfully added: " + new_book.title;
    } else {
        DebugLogger::log("CRITICAL: db_manager.AddBook failed for " + new_book.title);
        fs::remove(dest_p);
        return "Error: Failed to add book to database.";
    }
}

bool LibraryManager::DeleteBook(const std::string& book_uuid, DatabaseManager& db_manager, DeleteScope scope) {
    auto book_opt = db_manager.GetBookByUUID(book_uuid);
    if (!book_opt) {
        DebugLogger::log("Delete failed: Could not find book with UUID " + book_uuid);
        return false;
    }
    Book book_to_delete = *book_opt;

    bool file_deleted = false;
    if (!book_to_delete.path.empty() && fs::exists(book_to_delete.path)) {
        try {
            fs::remove(book_to_delete.path);
            DebugLogger::log("Successfully deleted local file: " + book_to_delete.path);
            file_deleted = true;
        } catch (const fs::filesystem_error& e) {
            DebugLogger::log("Error: Failed to delete file " + book_to_delete.path + ". Error: " + e.what());
            // Continue to process DB record even if file deletion fails
        }
    }

    bool db_success = false;
    switch (scope) {
        case DeleteScope::LocalOnly:
            // If the book has a cloud presence, mark it as cloud-only.
            // Otherwise, if it's a purely local book, delete the record entirely.
            if (!book_to_delete.google_drive_file_id.empty()) {
                db_success = db_manager.UpdateBookToCloudOnly(book_uuid);
                DebugLogger::log("Updated book " + book_uuid + " to cloud-only.");
            } else {
                db_success = db_manager.DeleteBook(book_uuid);
                DebugLogger::log("Deleted local-only book record " + book_uuid);
            }
            break;

        case DeleteScope::CloudAndLocal:
            // This scope means delete everything, everywhere.
            // The cloud part is handled by SyncController. Here we just delete the DB record.
            db_success = db_manager.DeleteBook(book_uuid);
            DebugLogger::log("Deleted book record " + book_uuid + " for CloudAndLocal scope.");
            break;
        
        case DeleteScope::CloudOnly:
            // This case is handled by SyncController, which calls db_manager.UpdateBookToLocalOnly.
            // LibraryManager should not be called with this scope directly.
            // We'll treat it as a no-op for the database record here for safety.
            DebugLogger::log("Warning: LibraryManager::DeleteBook called with CloudOnly scope. No DB action taken.");
            db_success = true; // No error, but no action.
            break;
    }

    if (!db_success) {
        DebugLogger::log("CRITICAL: DB record update/removal failed for book_uuid " + book_uuid);
        return false;
    }

    DebugLogger::log("Successfully processed book record for deletion for book_uuid " + book_uuid);
    return true;
}
