#include "sha256.h"

SHA256::SHA256() {
    // Constructor can be empty
}

void SHA256::add(const void* data, size_t len) {
    const picosha2::byte_t* bytes = static_cast<const picosha2::byte_t*>(data);
    buffer_.insert(buffer_.end(), bytes, bytes + len);
}

std::string SHA256::getHash() {
    std::string hash_hex_str;
    picosha2::hash256_hex_string(buffer_, hash_hex_str);
    reset(); // Automatically reset after getting the hash
    return hash_hex_str;
}

void SHA256::reset() {
    buffer_.clear();
}
