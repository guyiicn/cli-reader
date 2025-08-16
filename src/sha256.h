#ifndef SHA256_H
#define SHA256_H

#include <string>
#include <vector>
#include "picosha2.h"

class SHA256 {
public:
    SHA256();
    void add(const void* data, size_t len);
    std::string getHash();
    void reset();

private:
    std::vector<picosha2::byte_t> buffer_;
};

#endif // SHA256_H