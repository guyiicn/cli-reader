#ifndef BOOK_VIEW_MODEL_H
#define BOOK_VIEW_MODEL_H

#include "IBookParser.h"
#include "ftxui/dom/elements.hpp"
#include <vector>
#include <string>

using namespace ftxui;

// Represents a single page, pointing to a range of lines within the global line vector.
struct Page {
    size_t start_line_index;
    size_t end_line_index;
};

class BookViewModel {
public:
    BookViewModel(std::unique_ptr<IBookParser> parser);

    void Paginate(int width, int height);
    Elements GetPageContent(int page_index, int width);
    int GetTotalPages() const;
    std::string GetPageTitleForPage(int page_index);
    int GetChapterStartPage(int chapter_index) const;
    const std::vector<BookChapter>& GetChapters() const;
    const std::vector<BookChapter>& GetFlatChapters() const;

private:
    std::unique_ptr<IBookParser> parser_;
    std::vector<BookChapter> flat_chapters_; // A flattened list of all chapters, including children
    std::vector<std::string> all_lines_; // All lines from all chapters, concatenated.
    std::vector<Page> pages_;
    std::vector<int> page_to_chapter_index_; // Maps a page index to its chapter index in the flat_chapters_ list
    std::vector<int> chapter_to_start_page_; // Maps a chapter index in the flat_chapters_ list to its start page

    // PDF-specific handling
    bool is_pdf_ = false;
    int total_pages_ = 0;
};

#endif // BOOK_VIEW_MODEL_H
