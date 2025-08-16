#ifndef BOOK_H
#define BOOK_H

#include <string>
#include <vector>
#include <chrono>

// This structure defines the core data for a book.
// It is designed to align closely with the `books` table in the database,
// supporting both local metadata and cloud synchronization fields.
struct Book {
    // --- Core Identifiers ---
    std::string uuid; // Primary key for synchronization
    std::string title;
    std::string author;
    std::string path; // Local file path, NULL if cloud-only
    std::string hash; // SHA256 hash of the local file, NULL if cloud-only

    // --- Reading Progress ---
    int current_page = 0;
    int total_pages = 0;
    time_t add_date = 0;
    time_t last_read_time = 0;

    // --- Extended Metadata ---
    std::string cover_image_path;
    std::string format; // e.g., "PDF", "EPUB"

    // --- PDF-specific Status Fields ---
    std::string pdf_health_status = "unchecked";
    std::string pdf_content_type = "unknown";

    // --- OCR Status Fields ---
    std::string ocr_status = "none";

    // --- Cloud Sync Fields ---
    std::string sync_status = "local"; // "local", "cloud", "synced"
    std::string google_drive_file_id;
};

#endif // BOOK_H
