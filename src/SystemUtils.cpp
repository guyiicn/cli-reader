#include "SystemUtils.h"
#include "DebugLogger.h"
#include "picosha2.h"
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdlib> // For getenv

std::string SystemUtils::GetHomePath() {
    #ifdef _WIN32
        const char* home_drive = getenv("HOMEDRIVE");
        const char* home_path_env = getenv("HOMEPATH");
        if (home_drive && home_path_env) {
            return std::string(home_drive) + std::string(home_path_env);
        }
    #else
        const char* home_dir = getenv("HOME");
        if (home_dir) {
            return std::string(home_dir);
        }
    #endif
    return "";
}

std::string SystemUtils::ExecuteCommand(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::string SystemUtils::CalculateFileHash(const std::string& file_path) {
    std::ifstream file_stream(file_path, std::ios::binary);
    if (!file_stream) {
        DebugLogger::log("ERROR: Could not open file for hashing: " + file_path);
        return "";
    }
    std::vector<char> file_buffer((std::istreambuf_iterator<char>(file_stream)), std::istreambuf_iterator<char>());
    return picosha2::hash256_hex_string(file_buffer);
}

std::string SystemUtils::ExpandTilde(const std::string& path) {
    if (path.empty() || path[0] != '~') {
        return path;
    }
    std::string home = GetHomePath();
    if (home.empty()) {
        return path; // Cannot expand if home is not found
    }
    if (path.length() == 1 || path[1] == '/') {
        return home + path.substr(1);
    }
    return path; // Not a tilde expansion
}

std::string SystemUtils::ExpandEnvironmentVariables(const std::string& path) {
    if (path.find("$") == std::string::npos) {
        return path;
    }
    std::string result = path;
    // A simple implementation for $HOME on non-Windows
    #ifndef _WIN32
    size_t pos = result.find("$HOME");
    if (pos != std::string::npos) {
        std::string home = GetHomePath();
        if (!home.empty()) {
            result.replace(pos, 5, home);
        }
    }
    #endif
    // More complex logic for other variables could be added here
    return result;
}

fs::path SystemUtils::NormalizePath(const std::string& user_input) {
    std::string expanded_path = ExpandTilde(user_input);
    expanded_path = ExpandEnvironmentVariables(expanded_path);
    return fs::absolute(expanded_path).lexically_normal();
}

std::string SystemUtils::get_file_extension(const std::string& filename) {
    fs::path path(filename);
    std::string ext = path.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        return ext.substr(1);
    }
    return ""; // Return empty if no extension or extension doesn't start with a dot
}
