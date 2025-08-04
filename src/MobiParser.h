#ifndef MOBI_PARSER_H
#define MOBI_PARSER_H

#include "IBookParser.h"
#include <string>
#include <vector>
#include <memory>

class MobiParser : public IBookParser {
public:
    MobiParser(const std::string& file_path);
    ~MobiParser() override;

    std::string GetTitle() const override;
    std::string GetAuthor() const override;
    std::string GetType() const override;
    std::string GetFilePath() const override;
    const std::vector<BookChapter>& GetChapters() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

#endif // MOBI_PARSER_H
