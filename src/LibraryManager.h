#ifndef LIBRARY_MANAGER_H
#define LIBRARY_MANAGER_H

#include "DatabaseManager.h"
#include <string>
#include <vector>

class LibraryManager {
public:
    LibraryManager();

    std::string GetLibraryPath() const;
    std::string AddBook(const std::string& source_path, DatabaseManager& db_manager, int screen_w, int screen_h);
    bool DeleteBook(int book_id, DatabaseManager& db_manager);

private:
    std::string library_path_;
    void EnsureLibraryExists() const;
};

#endif // LIBRARY_MANAGER_H
