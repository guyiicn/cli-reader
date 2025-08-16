#include "PdfParser.h"
#include "DebugLogger.h"
#include <poppler-document.h>
#include <poppler-page.h>
#include <memory>
#include <filesystem>

namespace fs = std::filesystem;

// Helper to convert poppler strings to std::string, safely handling potential nulls.
static std::string to_std_string(const poppler::ustring& ustr) {
    auto utf8_bytes = ustr.to_utf8();
    if (utf8_bytes.empty()) {
        return "";
    }
    return std::string(utf8_bytes.data(), utf8_bytes.size());
}

PdfParser::PdfParser(const std::string& file_path)
    : file_path_(file_path) {
    DebugLogger::log("PdfParser instance created for: " + file_path_);
    // Constructor no longer loads the document.
}

bool PdfParser::Load() {
    DebugLogger::log("PdfParser: Calling poppler::document::load_from_file... This may take time.");
    doc_ = std::unique_ptr<poppler::document>(poppler::document::load_from_file(file_path_));
    DebugLogger::log("PdfParser: poppler::document::load_from_file finished.");
    
    if (!doc_ || doc_->is_locked()) {
        DebugLogger::log("PdfParser: Failed to load or locked PDF.");
        return false;
    }
    parseMetadata();
    return true;
}

PdfParser::~PdfParser() = default;

void PdfParser::parseMetadata() {
    if (!doc_) return;

    DebugLogger::log("PdfParser: Parsing metadata...");
    title_ = to_std_string(doc_->get_title());
    author_ = to_std_string(doc_->get_author());
    DebugLogger::log("PdfParser: Raw Title: '" + title_ + "', Raw Author: '" + author_ + "'");

    if (title_.empty()) {
        title_ = fs::path(file_path_).stem().string();
        DebugLogger::log("PdfParser: Title empty, fallback to filename: " + title_);
    }
    if (author_.empty()) {
        author_ = "Unknown Author";
    }
}

std::string PdfParser::GetTitle() const {
    return title_;
}

std::string PdfParser::GetAuthor() const {
    return author_;
}

std::string PdfParser::GetType() const {
    return "PDF";
}

std::string PdfParser::GetFilePath() const {
    return file_path_;
}

const std::vector<BookChapter>& PdfParser::GetChapters() const {
    // Return empty vector as we now use a page-based approach for PDFs.
    return chapters_;
}

int PdfParser::GetTotalPages() {
    if (!doc_) return 0;
    if (total_pages_ == -1) {
        total_pages_ = doc_->pages();
    }
    return total_pages_;
}

std::string PdfParser::GetTextForPage(int page_num) {
    if (!doc_ || page_num < 0 || page_num >= GetTotalPages()) {
        return "";
    }

    // Check cache first
    auto it = page_text_cache_.find(page_num);
    if (it != page_text_cache_.end()) {
        return it->second;
    }

    // Not in cache, so parse, cache, and return
    DebugLogger::log("PdfParser: Lazily parsing text for page " + std::to_string(page_num));
    std::unique_ptr<poppler::page> p(doc_->create_page(page_num));
    if (!p) {
        DebugLogger::log("PdfParser: Failed to create page object for page " + std::to_string(page_num));
        return "";
    }

    std::string page_text = to_std_string(p->text());
    page_text_cache_[page_num] = page_text;
    return page_text;
}
