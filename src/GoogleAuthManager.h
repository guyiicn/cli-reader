#pragma once

#include "ConfigManager.h"
#include <string>
#include "nlohmann/json.hpp"

class GoogleAuthManager {
public:
    explicit GoogleAuthManager(ConfigManager& config_manager);

    std::string get_access_token(bool& needs_user_interaction);
    std::string get_authorization_url();
    bool exchange_code_for_token(const std::string& auth_code);

private:
    std::string refresh_access_token();

    ConfigManager& config_manager_;
    std::string access_token_;
};
