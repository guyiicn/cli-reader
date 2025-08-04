/*
    picosha2.h
    Open-source license: MIT
    Copyright (c) 2017-2021 okdshin
*/
#ifndef PICOSHA2_H
#define PICOSHA2_H
//- The MIT License (MIT)
//- 
//- Copyright (c) 2017-2021 okdshin
//- 
//- Permission is hereby granted, free of charge, to any person obtaining a copy
//- of this software and associated documentation files (the "Software"), to deal
//- in the Software without restriction, including without limitation the rights
//- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//- copies of the Software, and to permit persons to whom the Software is
//- furnished to do so, subject to the following conditions:
//- 
//- The above copyright notice and this permission notice shall be included in all
//- copies or substantial portions of the Software.
//- 
//- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//- SOFTWARE.

#include <algorithm>
#include <cassert>
#include <iterator>
#include <sstream>
#include <vector>
#include <iomanip>

namespace picosha2
{
typedef unsigned long long word_t;
typedef unsigned char byte_t;

namespace detail
{
inline byte_t mask_8bit(byte_t x){
    return x & 0xff;
}

inline word_t mask_32bit(word_t x){
    return x & 0xffffffff;
}

const size_t K_size = 64;
const word_t K[K_size] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

template<typename RaIter>
inline word_t rotr(word_t x, std::size_t n){
    assert(n < 32);
    return mask_32bit((x >> n) | (x << (32 - n)));
}

template<typename RaIter>
inline word_t ch(word_t x, word_t y, word_t z){
    return (x & y) ^ (~x & z);
}

template<typename RaIter>
inline word_t maj(word_t x, word_t y, word_t z){
    return (x & y) ^ (x & z) ^ (y & z);
}

template<typename RaIter>
inline word_t sigma0(word_t x){
    return rotr<RaIter>(x, 2) ^ rotr<RaIter>(x, 13) ^ rotr<RaIter>(x, 22);
}

template<typename RaIter>
inline word_t sigma1(word_t x){
    return rotr<RaIter>(x, 6) ^ rotr<RaIter>(x, 11) ^ rotr<RaIter>(x, 25);
}

template<typename RaIter>
inline word_t a(word_t x){
    return rotr<RaIter>(x, 7) ^ rotr<RaIter>(x, 18) ^ (x >> 3);
}

template<typename RaIter>
inline word_t b(word_t x){
    return rotr<RaIter>(x, 17) ^ rotr<RaIter>(x, 19) ^ (x >> 10);
}

template<typename RaIter>
class hash_appender
{
public:
    hash_appender(word_t* H) : H_(H){}
    
    void operator()(const byte_t* data, std::size_t n){
        word_t w[64];
        std::fill(w, w + 64, 0);
        for(std::size_t i = 0; i < n; ++i){
            w[i/4] |= static_cast<word_t>(mask_8bit(data[i])) << (24 - 8*(i%4));
        }
        
        for(std::size_t i = 16; i < 64; ++i){
            w[i] = mask_32bit(b<RaIter>(w[i-2]) + w[i-7] + a<RaIter>(w[i-15]) + w[i-16]);
        }
        
        word_t a = H_[0];
        word_t b = H_[1];
        word_t c = H_[2];
        word_t d = H_[3];
        word_t e = H_[4];
        word_t f = H_[5];
        word_t g = H_[6];
        word_t h = H_[7];
        
        for(std::size_t i = 0; i < 64; ++i){
            word_t temp1 = h + sigma1<RaIter>(e) + ch<RaIter>(e, f, g) + K[i] + w[i];
            word_t temp2 = sigma0<RaIter>(a) + maj<RaIter>(a, b, c);
            h = g;
            g = f;
            f = e;
            e = mask_32bit(d + temp1);
            d = c;
            c = b;
            b = a;
            a = mask_32bit(temp1 + temp2);
        }
        H_[0] = mask_32bit(H_[0] + a);
        H_[1] = mask_32bit(H_[1] + b);
        H_[2] = mask_32bit(H_[2] + c);
        H_[3] = mask_32bit(H_[3] + d);
        H_[4] = mask_32bit(H_[4] + e);
        H_[5] = mask_32bit(H_[5] + f);
        H_[6] = mask_32bit(H_[6] + g);
        H_[7] = mask_32bit(H_[7] + h);
    }
private:
    word_t* H_;
};

} // namespace detail

template<typename RaIter>
inline void
process(RaIter first, RaIter last, detail::hash_appender<RaIter>& appender){
    std::size_t N_bits = std::distance(first, last) * 8;
    std::size_t L = N_bits / 8;
    std::size_t K = (448 - (L*8 + 1)) % 512;
    if(K < 0){
        K += 512;
    }
    K /= 8;
    
    byte_t M[64];
    std::fill(M, M + 64, 0);
    
    std::size_t i = 0;
    for(RaIter it = first; it != last; ++it, ++i){
        M[i%64] = *it;
        if(i%64 == 63){
            appender(M, 64);
            std::fill(M, M + 64, 0);
        }
    }
    M[i%64] = 0x80;
    if(i%64 > 55){
        appender(M, 64);
        std::fill(M, M + 64, 0);
    }
    
    byte_t N_data[8];
    for(int j = 0; j < 8; ++j){
        N_data[j] = static_cast<byte_t>(N_bits >> (56 - 8*j));
    }
    
    for(int j = 0; j < 8; ++j){
        M[56+j] = N_data[j];
    }
    appender(M, 64);
}

template<typename RaIter>
inline void get_hash_bytes(RaIter first, RaIter last, byte_t* hash){
    word_t H[] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    
    detail::hash_appender<RaIter> appender(H);
    process(first, last, appender);
    
    for(int i = 0; i < 8; ++i){
        for(int j = 0; j < 4; ++j){
            hash[i*4+j] = detail::mask_8bit(H[i] >> (24 - 8*j));
        }
    }
}

inline void bytes_to_hex_string(const byte_t* bytes, std::size_t n, std::string& hex_str){
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for(std::size_t i = 0; i < n; ++i){
        ss << std::setw(2) << static_cast<unsigned int>(bytes[i]);
    }
    hex_str = ss.str();
}

template<typename RaIter>
inline std::string get_hash_hex_string(RaIter first, RaIter last){
    byte_t hash[32];
    get_hash_bytes(first, last, hash);
    std::string hex_str;
    bytes_to_hex_string(hash, 32, hex_str);
    return hex_str;
}

inline void get_hash_hex_string(std::istream& is, std::string& hex_str){
    std::vector<byte_t> vec;
    std::copy(std::istreambuf_iterator<char>(is),
              std::istreambuf_iterator<char>(),
              std::back_inserter(vec));
    hex_str = get_hash_hex_string(vec.begin(), vec.end());
}

} // namespace picosha2

#endif //PICOSHA2_H
