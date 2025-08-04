
#ifndef TXT_PARSER_H
#define TXT_PARSER_H

#include "IBookParser.h"
#include <string>
#include <vector>

class TxtParser : public IBookParser {
public:
    TxtParser(const std::string& file_path);

    bool isOpen() const;
    std::string GetTitle() const override;
    std::string GetAuthor() const override;
    std::string GetType() const override;
    std::string GetFilePath() const override;
    const std::vector<BookChapter>& GetChapters() const override;

private:
    bool is_open_ = false;
    std::string file_path_;
    std::vector<BookChapter> chapters_;
};

#endif // TXT_PARSER_H
