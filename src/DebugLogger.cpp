
#include "DebugLogger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

std::ofstream DebugLogger::log_stream_;

void DebugLogger::init(const std::string& log_file) {
    log_stream_.open(log_file, std::ios::out | std::ios::trunc);
    if (!log_stream_.is_open()) {
        std::cerr << "Failed to open log file: " << log_file << std::endl;
    }
}

void DebugLogger::log(const std::string& message) {
    if (!log_stream_.is_open()) return;

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    log_stream_ << "[" << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X") << "] "
                << message << std::endl;
}
