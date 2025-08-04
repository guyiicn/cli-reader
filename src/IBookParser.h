#ifndef IBOOK_PARSER_H
#define IBOOK_PARSER_H

#include <string>
#include <vector>
#include <memory>

// A universal, format-agnostic representation of a book chapter.
struct BookChapter {
    std::string title;
    std::vector<std::string> paragraphs;
    std::vector<std::string> lines; // Populated by BookViewModel
    std::vector<BookChapter> children; // For nested chapters in TOC
};

// An abstract interface for all book parser types.
class IBookParser {
public:
    virtual ~IBookParser() = default;

    virtual std::string GetTitle() const = 0;
    virtual std::string GetAuthor() const = 0;
    virtual std::string GetType() const = 0;
    virtual std::string GetFilePath() const = 0;
    virtual const std::vector<BookChapter>& GetChapters() const = 0;
};

#endif // IBOOK_PARSER_H
