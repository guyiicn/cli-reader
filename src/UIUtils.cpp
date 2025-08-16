#include "UIUtils.h"
#include "EpubParser.h"
#include "MobiParser.h"
#include "PdfParser.h"
#include "TxtParser.h"
#include <algorithm>
#include <filesystem>
#include <iostream>

bool promptForGoogleCredentials(ConfigManager& configManager) {
    std::string clientId, clientSecret;

    // Clear the screen or provide spacing
    std::cout << "\n\n--- Google Drive Sync Setup ---\n" << std::endl;
    
    std::cout << "To enable Google Drive synchronization, you need to provide your own API credentials." << std::endl;
    std::cout << "Please follow these steps:" << std::endl;
    std::cout << "1. Go to the Google Cloud Console: https://console.cloud.google.com/" << std::endl;
    std::cout << "2. Create a new project or select an existing one." << std::endl;
    std::cout << "3. Enable the 'Google Drive API' for your project." << std::endl;
    std::cout << "4. Go to 'Credentials', click 'Create Credentials', and choose 'OAuth client ID'." << std::endl;
    std::cout << "5. Select 'Desktop app' as the application type." << std::endl;
    std::cout << "6. Copy the generated 'Client ID' and 'Client Secret' below." << std::endl;
    std::cout << "Your credentials will be stored locally in your configuration file and will not be shared." << std::endl;
    std::cout << "\nPress Enter to continue, or type 'skip' to cancel." << std::endl;

    std::string input;
    std::getline(std::cin, input);
    if (input == "skip") {
        std::cout << "Setup skipped. You can configure sync later by pressing 'c' in the library view." << std::endl;
        std::cout << "Press Enter to continue." << std::endl;
        std::getline(std::cin, input);
        return false;
    }

    std::cout << "\nPlease enter your Google Client ID: ";
    std::getline(std::cin, clientId);

    std::cout << "Please enter your Google Client Secret: ";
    std::getline(std::cin, clientSecret);

    if (clientId.empty() || clientSecret.empty()) {
        std::cout << "\nClient ID or Secret cannot be empty. Setup failed." << std::endl;
        std::cout << "Press Enter to continue." << std::endl;
        std::getline(std::cin, input);
        return false;
    }

    configManager.setGoogleCredentials(clientId, clientSecret);
    std::cout << "\nCredentials saved successfully!" << std::endl;
    std::cout << "Press Enter to continue." << std::endl;
    std::getline(std::cin, input);

    return true;
}

namespace fs = std::filesystem;

// --- Parser Factory ---
std::unique_ptr<IBookParser> CreateParser(const std::string& path) {

    std::string extension = fs::path(path).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (extension == ".epub") return std::make_unique<EpubParser>(path);
    if (extension == ".txt") return std::make_unique<TxtParser>(path);
    if (extension == ".mobi" || extension == ".azw3") return std::make_unique<MobiParser>(path);
    if (extension == ".pdf") return std::make_unique<PdfParser>(path);
    return nullptr;
}

// --- Helper Functions ---
void FlattenChapters(const std::vector<BookChapter>& chapters, std::vector<std::string>& entries, int depth) {
    for (const auto& chapter : chapters) {
        std::string prefix = std::string(depth * 2, ' ');
        entries.push_back(prefix + chapter.title);
        FlattenChapters(chapter.children, entries, depth + 1);
    }
}

void SortEntries(std::vector<std::string>& entries, const fs::path& p) {
    std::sort(entries.begin(), entries.end(),
              [&](const std::string& a, const std::string& b) {
                  bool is_a_dir = fs::is_directory(p / a);
                  bool is_b_dir = fs::is_directory(p / b);
                  if (is_a_dir != is_b_dir) return is_a_dir;
                  return a < b;
              });
}

void UpdatePickerEntries(const fs::path& p, std::vector<std::string>& entries, int& selected_entry) {
    entries.clear();
    selected_entry = 0;
    try {
        if (fs::exists(p) && fs::is_directory(p)) {
            entries.push_back("../");
            std::vector<std::string> child_entries;
            for (const auto& entry : fs::directory_iterator(p)) {
                try {
                    std::string filename = entry.path().filename().string();
                    if (fs::is_directory(entry.path())) {
                        child_entries.push_back(filename + "/");
                    } else {
                        std::string ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".epub" || ext == ".txt" || ext == ".mobi" || ext == ".azw3" || ext == ".pdf") {
                            child_entries.push_back(filename);
                        }
                    }
                } catch (const fs::filesystem_error&) {}
            }
            SortEntries(child_entries, p);
            entries.insert(entries.end(), child_entries.begin(), child_entries.end());
        }
    } catch (const fs::filesystem_error&) {
        entries.push_back("Error accessing directory.");
    }
}