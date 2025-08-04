
#ifndef EPUB_PARSER_H
#define EPUB_PARSER_H

#include "IBookParser.h"
#include <string>
#include <vector>
#include <memory>

class EpubParser : public IBookParser {
public:
    EpubParser(const std::string& file_path);
    ~EpubParser() override;

    bool isOpen() const;
    std::string GetTitle() const override;
    std::string GetAuthor() const override;
    std::string GetType() const override;
    std::string GetFilePath() const override;
    const std::vector<BookChapter>& GetChapters() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

#endif // EPUB_PARSER_H
