#ifndef SYSTEM_UTILS_H
#define SYSTEM_UTILS_H

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace SystemUtils {
    std::string GetHomePath();
    std::string ExpandTilde(const std::string& path);
    std::string ExpandEnvironmentVariables(const std::string& path);
    fs::path NormalizePath(const std::string& user_input);
    
    std::string ExecuteCommand(const std::string& cmd);
    std::string CalculateFileHash(const std::string& file_path);
    std::string get_file_extension(const std::string& filename);
}

#endif // SYSTEM_UTILS_H
