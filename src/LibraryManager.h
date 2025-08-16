#ifndef LIBRARY_MANAGER_H
#define LIBRARY_MANAGER_H

#include "DatabaseManager.h"
#include "CommonTypes.h"
#include "ConfigManager.h"
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

class LibraryManager {
public:
    explicit LibraryManager(const ConfigManager& config_manager);

    std::string AddBook(const std::string& source_path, DatabaseManager& db_manager, int screen_w, int screen_h);
    bool DeleteBook(const std::string& book_uuid, DatabaseManager& db_manager, DeleteScope scope);

private:
    fs::path library_path_;
    void EnsureLibraryExists() const;
    void PerformPdfPreflight(Book& book);
};

#endif // LIBRARY_MANAGER_H
