
#ifndef DEBUG_LOGGER_H
#define DEBUG_LOGGER_H

#include <string>
#include <fstream>

class DebugLogger {
public:
    static void init(const std::string& log_file);
    static void log(const std::string& message);

private:
    static std::ofstream log_stream_;
};

#endif // DEBUG_LOGGER_H
