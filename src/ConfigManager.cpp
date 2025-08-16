#include "ConfigManager.h"
#include "DebugLogger.h"
#include <filesystem>

namespace fs = std::filesystem;

ConfigManager::ConfigManager(DatabaseManager& db_manager) : db_manager_(db_manager) {}

void ConfigManager::LoadSettings() {
    DebugLogger::log("Loading settings into ConfigManager...");
    settings_ = db_manager_.GetAllSettings();
    DebugLogger::log("Settings loaded. Found " + std::to_string(settings_.size()) + " key-value pairs.");
}

std::string ConfigManager::Get(const std::string& key) const {
    auto it = settings_.find(key);
    if (it != settings_.end()) {
        return it->second;
    }
    DebugLogger::log("WARN: Setting key not found: " + key);
    return "";
}

// --- Type-safe getters ---

fs::path ConfigManager::GetLibraryPath() const {
    return fs::path(Get("library_path"));
}

fs::path ConfigManager::GetConfigPath() const {
    return fs::path(Get("database_path"));
}

std::string ConfigManager::GetClientId() const {
    return Get("client_id");
}

std::string ConfigManager::GetClientSecret() const {
    return Get("client_secret");
}

fs::path ConfigManager::GetLastPickerPath() const {
    return fs::path(Get("last_picker_path"));
}

std::string ConfigManager::GetRefreshToken() const {
    return Get("refresh_token");
}

void ConfigManager::SetRefreshToken(const std::string& token) {
    settings_["refresh_token"] = token;
    db_manager_.SetSetting("refresh_token", token);
}

void ConfigManager::SetLastPickerPath(const fs::path& path) {
    settings_["last_picker_path"] = path.string();
    db_manager_.SetSetting("last_picker_path", path.string());
}

void ConfigManager::setGoogleCredentials(const std::string& clientId, const std::string& clientSecret) {
    settings_["client_id"] = clientId;
    settings_["client_secret"] = clientSecret;
    db_manager_.SetSetting("client_id", clientId);
    db_manager_.SetSetting("client_secret", clientSecret);
}

std::string ConfigManager::getGoogleClientId() const {
    return Get("client_id");
}

std::string ConfigManager::getGoogleClientSecret() const {
    return Get("client_secret");
}

bool ConfigManager::hasGoogleCredentials() const {
    return !Get("client_id").empty() && !Get("client_secret").empty();
}
