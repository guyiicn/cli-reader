#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <map>
#include <string>
#include <filesystem>
#include "DatabaseManager.h"

namespace fs = std::filesystem;

class ConfigManager {
public:
    explicit ConfigManager(DatabaseManager& db_manager);
    void LoadSettings();
    std::string Get(const std::string& key) const;

    // Type-safe getters
    fs::path GetLibraryPath() const;
    fs::path GetConfigPath() const;
    std::string GetClientId() const;
    std::string GetClientSecret() const;
    fs::path GetLastPickerPath() const;
    std::string GetRefreshToken() const;
    void SetRefreshToken(const std::string& token);
    void SetLastPickerPath(const fs::path& path);

    // New methods for Google Credentials
    void setGoogleCredentials(const std::string& clientId, const std::string& clientSecret);
    std::string getGoogleClientId() const;
    std::string getGoogleClientSecret() const;
    bool hasGoogleCredentials() const;

private:
    DatabaseManager& db_manager_;
    std::map<std::string, std::string> settings_;
};

#endif // CONFIG_MANAGER_H