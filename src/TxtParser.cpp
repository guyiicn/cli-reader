#include "TxtParser.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

TxtParser::TxtParser(const std::string& file_path) : file_path_(file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open txt file: " << file_path << std::endl;
        is_open_ = false;
        return;
    }

    BookChapter chapter;
    chapter.title = fs::path(file_path).stem().string();
    
    std::string line;
    std::stringstream current_paragraph;

    while (std::getline(file, line)) {
        if (line.empty()) {
            if (current_paragraph.tellp() > 0) {
                chapter.paragraphs.push_back(current_paragraph.str());
                current_paragraph.str("");
                current_paragraph.clear();
            }
        } else {
            current_paragraph << line << "\n";
        }
    }

    if (current_paragraph.tellp() > 0) {
        chapter.paragraphs.push_back(current_paragraph.str());
    }
    
    chapters_.push_back(chapter);
    is_open_ = true;
}

bool TxtParser::isOpen() const {
    return is_open_;
}

std::string TxtParser::GetTitle() const {
    if (chapters_.empty()) return "Unknown Title";
    return chapters_[0].title;
}

std::string TxtParser::GetAuthor() const {
    return "Unknown Author";
}

std::string TxtParser::GetType() const {
    return "TXT";
}

std::string TxtParser::GetFilePath() const {
    return file_path_;
}

const std::vector<BookChapter>& TxtParser::GetChapters() const {
    return chapters_;
}
