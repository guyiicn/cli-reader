#include "GoogleAuthManager.h"
#include "DebugLogger.h"
#include <cpr/cpr.h>

GoogleAuthManager::GoogleAuthManager(ConfigManager& config_manager) 
    : config_manager_(config_manager) {}

std::string GoogleAuthManager::get_access_token(bool& needs_user_interaction) {
    needs_user_interaction = false;
    if (!access_token_.empty()) {
        // TODO: Check for token expiry
        return access_token_;
    }

    std::string refresh_token = config_manager_.GetRefreshToken();
    if (!refresh_token.empty()) {
        access_token_ = refresh_access_token();
        if (!access_token_.empty()) {
            return access_token_;
        }
    }
    
    // If we are here, we need user to log in.
    needs_user_interaction = true;
    return ""; 
}

std::string GoogleAuthManager::get_authorization_url() {
    const std::string client_id = config_manager_.GetClientId();
    if (client_id.empty()) {
        DebugLogger::log("CRITICAL: Client ID is not configured.");
        return "";
    }
    const std::string auth_uri = "https://accounts.google.com/o/oauth2/v2/auth";
    const std::string scope = "https://www.googleapis.com/auth/drive.file";
    
    return auth_uri + "?client_id=" + client_id + 
           "&redirect_uri=http://localhost" + 
           "&response_type=code" + 
           "&scope=" + scope + 
           "&access_type=offline" + 
           "&prompt=consent";
}

bool GoogleAuthManager::exchange_code_for_token(const std::string& auth_code) {
    const std::string client_id = config_manager_.GetClientId();
    const std::string client_secret = config_manager_.GetClientSecret();

    cpr::Response r = cpr::Post(cpr::Url{"https://oauth2.googleapis.com/token"},
                                cpr::Payload{
                                    {"client_id", client_id},
                                    {"client_secret", client_secret},
                                    {"code", auth_code},
                                    {"grant_type", "authorization_code"},
                                    {"redirect_uri", "http://localhost"}
                                });

    if (r.status_code == 200) {
        nlohmann::json token_data = nlohmann::json::parse(r.text);
        if (token_data.contains("refresh_token")) {
            std::string new_refresh_token = token_data["refresh_token"].get<std::string>();
            config_manager_.SetRefreshToken(new_refresh_token); // This updates the in-memory cache for now
            
            if (token_data.contains("access_token")) {
                access_token_ = token_data["access_token"].get<std::string>();
            }
            return true;
        }
    }
    DebugLogger::log("ERROR: Failed to exchange code for token. Response: " + r.text);
    return false;
}

std::string GoogleAuthManager::refresh_access_token() {
    cpr::Response r = cpr::Post(cpr::Url{"https://oauth2.googleapis.com/token"},
                                cpr::Payload{
                                    {"client_id", config_manager_.GetClientId()},
                                    {"client_secret", config_manager_.GetClientSecret()},
                                    {"refresh_token", config_manager_.GetRefreshToken()},
                                    {"grant_type", "refresh_token"}
                                });

    if (r.status_code == 200) {
        nlohmann::json token_data = nlohmann::json::parse(r.text);
        if (token_data.contains("access_token")) {
            return token_data["access_token"].get<std::string>();
        }
    }
    DebugLogger::log("ERROR: Failed to refresh access token. Response: " + r.text);
    return "";
}
