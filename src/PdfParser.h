#ifndef PDFPARSER_H
#define PDFPARSER_H

#include "IBookParser.h"
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace poppler {
    class document;
}

class PdfParser : public IBookParser {
public:
    explicit PdfParser(const std::string& file_path);
    ~PdfParser() override;

    bool Load(); // New method to perform the actual loading

    // IBookParser interface implementation
    std::string GetTitle() const override;
    std::string GetAuthor() const override;
    std::string GetType() const override;
    std::string GetFilePath() const override;
    const std::vector<BookChapter>& GetChapters() const override; // Will return an empty vector for now

    // New methods for lazy loading
    int GetTotalPages();
    std::string GetTextForPage(int page_num);
    bool IsImageBased() const;

private:
    void parseMetadata(); // Renamed for clarity

    std::string file_path_;
    std::string title_;
    std::string author_;
    std::vector<BookChapter> chapters_; // Kept for interface compliance, but will be empty
    std::unique_ptr<poppler::document> doc_;
    int total_pages_ = -1; // Cache for total pages
    std::map<int, std::string> page_text_cache_;
    bool is_image_based_ = false;
};

#endif // PDFPARSER_H
