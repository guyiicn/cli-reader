#include "BookViewModel.h"
#include "HtmlRenderer.h"
#include "DebugLogger.h"
#include <numeric>

using namespace ftxui;

// --- UTF-8 and Word Wrapping Utilities ---

// Modern UTF-8 conversion functions to replace deprecated std::codecvt_utf8
std::u32string utf8_to_u32(const std::string& utf8_str) {
    std::u32string result;
    size_t i = 0;
    while (i < utf8_str.size()) {
        unsigned char byte = utf8_str[i];
        char32_t codepoint = 0;
        
        if ((byte & 0x80) == 0) {
            codepoint = byte;
            i++;
        } else if ((byte & 0xE0) == 0xC0) {
            if (i + 1 >= utf8_str.size()) break;
            codepoint = ((byte & 0x1F) << 6) | (utf8_str[i + 1] & 0x3F);
            i += 2;
        } else if ((byte & 0xF0) == 0xE0) {
            if (i + 2 >= utf8_str.size()) break;
            codepoint = ((byte & 0x0F) << 12) | 
                       ((utf8_str[i + 1] & 0x3F) << 6) | 
                       (utf8_str[i + 2] & 0x3F);
            i += 3;
        } else if ((byte & 0xF8) == 0xF0) {
            if (i + 3 >= utf8_str.size()) break;
            codepoint = ((byte & 0x07) << 18) | 
                       ((utf8_str[i + 1] & 0x3F) << 12) | 
                       ((utf8_str[i + 2] & 0x3F) << 6) | 
                       (utf8_str[i + 3] & 0x3F);
            i += 4;
        } else {
            i++;
            continue;
        }
        result.push_back(codepoint);
    }
    return result;
}

std::string u32_to_utf8(const std::u32string& u32_str) {
    std::string result;
    for (char32_t codepoint : u32_str) {
        if (codepoint <= 0x7F) {
            result.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FF) {
            result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0xFFFF) {
            result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0x10FFFF) {
            result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }
    return result;
}
int character_display_width(uint32_t c) {
    if (c >= 0x4E00 && c <= 0x9FFF) return 2;
    if (c >= 0x3000 && c <= 0x303F) return 2;
    if (c >= 0xFF00 && c <= 0xFFEF) return 2;
    return 1;
}

std::vector<std::string> word_wrap(const std::string& text, int width) {
    std::vector<std::string> lines;
    if (text.empty()) {
        lines.push_back("");
        return lines;
    }
    if (width <= 0) {
        lines.push_back(text);
        return lines;
    }

    std::u32string u32_text = utf8_to_u32(text);

    size_t start = 0;
    while (start < u32_text.length()) {
        size_t end = start;
        int current_width = 0;
        size_t last_space = start;

        while (end < u32_text.length()) {
            char32_t c = u32_text[end];
            if (c == U'\n') {
                last_space = end;
                break;
            }

            current_width += character_display_width(c);
            if (current_width > width) {
                break;
            }

            if (c == U' ' || c == U'\t') {
                last_space = end;
            }
            end++;
        }

        if (end == u32_text.length()) {
            last_space = end;
        } else if (u32_text[end] == U'\n') {
            // Keep the newline character
        } else if (last_space == start) {
            last_space = end;
        }

        lines.push_back(u32_to_utf8(u32_text.substr(start, last_space - start)));
        
        start = last_space;
        if (start < u32_text.length() && (u32_text[start] == U' ' || u32_text[start] == U'\n')) {
            start++;
        }
    }
     if (start == u32_text.length() && !u32_text.empty() && u32_text.back() == U'\n') {
        lines.push_back("");
    }

    return lines;
}

// --- BookViewModel Implementation ---

// Helper to recursively flatten the chapter tree for pagination
void flatten_chapters_for_pagination(const std::vector<BookChapter>& chapters, std::vector<BookChapter>& flat_list) {
    for (const auto& chapter : chapters) {
        flat_list.push_back(chapter);
        flatten_chapters_for_pagination(chapter.children, flat_list);
    }
}

BookViewModel::BookViewModel(std::unique_ptr<IBookParser> parser) : parser_(std::move(parser)) {
    DebugLogger::log("BookViewModel created.");
    // When a view model is created, immediately prepare the flat chapter list for pagination
    flatten_chapters_for_pagination(parser_->GetChapters(), flat_chapters_);
}

void BookViewModel::Paginate(int width, int height) {
    /*
    // --- OLD PAGINATION LOGIC (Preserved for reference) ---
    all_lines_.clear();
    pages_.clear();
    page_to_chapter_index_.clear();
    chapter_to_start_page_.assign(flat_chapters_.size(), 0);
    DebugLogger::log("--- Starting Pagination ---");

    if (width <= 0 || height <= 0) return;

    std::vector<size_t> chapter_start_line_indices;
    size_t line_count = 0;

    // First, flatten all paragraphs into lines and record the start line of each chapter
    for (const auto& chapter : flat_chapters_) {
        chapter_start_line_indices.push_back(line_count);
        for (const auto& p_text : chapter.paragraphs) {
            auto wrapped_lines = word_wrap(p_text, width);
            all_lines_.insert(all_lines_.end(), wrapped_lines.begin(), wrapped_lines.end());
            line_count += wrapped_lines.size();
        }
        if (!chapter.paragraphs.empty()) {
            all_lines_.push_back("");
            line_count++;
        }
    }

    // Now, create pages and map them to chapters
    int current_chapter_idx = 0;
    for (size_t i = 0; i < all_lines_.size(); i += height) {
        while (current_chapter_idx + 1 < chapter_start_line_indices.size() &&
               i >= chapter_start_line_indices[current_chapter_idx + 1]) {
            current_chapter_idx++;
        }
        
        if (chapter_to_start_page_[current_chapter_idx] == 0 && i != 0) {
             chapter_to_start_page_[current_chapter_idx] = pages_.size();
        }
        if (current_chapter_idx == 0) {
            chapter_to_start_page_[0] = 0;
        }

        Page page;
        page.start_line_index = i;
        page.end_line_index = std::min(i + height, all_lines_.size());
        pages_.push_back(page);
        page_to_chapter_index_.push_back(current_chapter_idx);
    }
    */

    // --- NEW PAGINATION LOGIC ---
    all_lines_.clear();
    pages_.clear();
    page_to_chapter_index_.clear();
    chapter_to_start_page_.assign(flat_chapters_.size(), 0);
    DebugLogger::log("--- Starting New Pagination Logic ---");

    if (width <= 0 || height <= 0) return;

    size_t global_line_index = 0;

    for (int i = 0; i < flat_chapters_.size(); ++i) {
        const auto& chapter = flat_chapters_[i];
        
        // 1. Record the starting page for the current chapter.
        chapter_to_start_page_[i] = pages_.size();

        // 2. Generate all lines for the current chapter.
        std::vector<std::string> chapter_lines;
        for (const auto& p_text : chapter.paragraphs) {
            auto wrapped_lines = word_wrap(p_text, width);
            chapter_lines.insert(chapter_lines.end(), wrapped_lines.begin(), wrapped_lines.end());
        }
        // Add a blank line after a chapter if it has content, for spacing.
        if (!chapter.paragraphs.empty()) {
            chapter_lines.push_back(""); 
        }

        // 3. Paginate the current chapter's lines.
        if (chapter_lines.empty()) {
            // If a chapter is empty (e.g., a title-only entry), create a single blank page for it.
            Page page;
            page.start_line_index = global_line_index;
            all_lines_.push_back(""); // Add a single empty line to represent the page content
            page.end_line_index = ++global_line_index;
            pages_.push_back(page);
            page_to_chapter_index_.push_back(i);
        } else {
            for (size_t j = 0; j < chapter_lines.size(); j += height) {
                Page page;
                page.start_line_index = global_line_index;
                
                size_t end_of_page_in_chapter = std::min(j + height, chapter_lines.size());
                all_lines_.insert(all_lines_.end(), chapter_lines.begin() + j, chapter_lines.begin() + end_of_page_in_chapter);
                
                global_line_index += (end_of_page_in_chapter - j);
                page.end_line_index = global_line_index;
                
                pages_.push_back(page);
                page_to_chapter_index_.push_back(i);
            }
        }
    }

    DebugLogger::log("[Paginate] New pagination complete. Total pages created: " + std::to_string(pages_.size()));
}


Elements BookViewModel::GetPageContent(int page_index) {
    Elements page_elements;
    if (page_index < 0 || page_index >= pages_.size()) {
        return page_elements;
    }
    const auto& page = pages_[page_index];
    for(size_t i = page.start_line_index; i < page.end_line_index; ++i) {
        page_elements.push_back(text(all_lines_[i]));
    }
    return page_elements;
}

int BookViewModel::GetTotalPages() const {
    return pages_.size();
}

std::string BookViewModel::GetPageTitleForPage(int page_index) {
    if (page_index < 0 || page_index >= page_to_chapter_index_.size()) {
        return "Unknown Chapter";
    }
    int chapter_idx = page_to_chapter_index_[page_index];
    return flat_chapters_[chapter_idx].title;
}

int BookViewModel::GetChapterStartPage(int chapter_index) const {
    if (chapter_index < 0 || chapter_index >= chapter_to_start_page_.size()) {
        return 0;
    }
    return chapter_to_start_page_[chapter_index];
}

const std::vector<BookChapter>& BookViewModel::GetChapters() const {
    // Note: This returns the original hierarchical chapters for the TOC view
    return parser_->GetChapters();
}

const std::vector<BookChapter>& BookViewModel::GetFlatChapters() const {
    return flat_chapters_;
}
