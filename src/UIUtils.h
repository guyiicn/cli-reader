#ifndef UIUTILS_H
#define UIUTILS_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "IBookParser.h"
#include "ConfigManager.h"

// --- User Input Prompts ---
bool promptForGoogleCredentials(ConfigManager& configManager);

namespace fs = std::filesystem;

// --- Parser Factory ---
std::unique_ptr<IBookParser> CreateParser(const std::string& path);

// --- Helper Functions ---
void FlattenChapters(const std::vector<BookChapter>& chapters, std::vector<std::string>& entries, int depth = 0);
void SortEntries(std::vector<std::string>& entries, const fs::path& p);
void UpdatePickerEntries(const fs::path& p, std::vector<std::string>& entries, int& selected_entry);

#endif // UIUTILS_H