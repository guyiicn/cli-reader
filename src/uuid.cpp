#include "uuid.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace uuid {

std::string generate_uuid_v4() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (int i = 0; i < 4; ++i) ss << std::setw(2) << dis(gen);
    ss << "-";
    for (int i = 0; i < 2; ++i) ss << std::setw(2) << dis(gen);
    ss << "-4"; // Version 4
    ss << std::setw(3) << (dis(gen) & 0x0FFF | 0x4000);
    ss << "-";
    ss << std::setw(4) << (dis(gen) & 0x3FFF | 0x8000);
    ss << "-";
    for (int i = 0; i < 6; ++i) ss << std::setw(2) << dis(gen);

    return ss.str();
}

} // namespace uuid
