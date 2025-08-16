/*
The MIT License (MIT)

Copyright (C) 2017 okdshin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#ifndef PICOSHA2_H
#define PICOSHA2_H
//picosha2:20140213
#ifndef PICOSHA2_BUFFER_SIZE_FOR_INPUT_ITERATOR
#define PICOSHA2_BUFFER_SIZE_FOR_INPUT_ITERATOR 1048576 //=1024*1024: default is 1MB memory
#endif
#include <vector>
#include <iterator>
#include <cassert>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iomanip>

namespace picosha2
{
typedef unsigned long word_t;
typedef unsigned char byte_t;

static const size_t k_digest_size = 32;

namespace detail
{
inline byte_t mask_8bit(byte_t x){
	return x & 0xff;
}

inline word_t mask_32bit(word_t x){
	return x & 0xffffffff;
}

const word_t add_constant[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

const word_t initial_message_digest[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

inline word_t rotr(word_t x, std::size_t n){
	assert(n < 32);
	return mask_32bit((x >> n) | (x << (32 - n)));
}

inline word_t ch(word_t x, word_t y, word_t z){
	return (x & y) ^ ((~x) & z);
}

inline word_t maj(word_t x, word_t y, word_t z){
	return (x & y) ^ (x & z) ^ (y & z);
}

inline word_t sigma0(word_t x){
	return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

inline word_t sigma1(word_t x){
	return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

inline word_t delta0(word_t x){
	return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

inline word_t delta1(word_t x){
	return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

template<typename InIter>
void process_chunk(InIter first, InIter last, word_t* message_digest)
{
	assert(std::distance(first, last) == 64);
	word_t w[64];
	for(std::size_t i = 0; i < 16; ++i){
		w[i] = mask_32bit(static_cast<word_t>(mask_8bit(*first++)) << 24 |
						  static_cast<word_t>(mask_8bit(*first++)) << 16 |
						  static_cast<word_t>(mask_8bit(*first++)) << 8 |
						  static_cast<word_t>(mask_8bit(*first++)));
	}
	for(std::size_t i = 16; i < 64; ++i){
		w[i] = mask_32bit(delta1(w[i-2]) + w[i-7] + delta0(w[i-15]) + w[i-16]);
	}

	word_t a = message_digest[0];
	word_t b = message_digest[1];
	word_t c = message_digest[2];
	word_t d = message_digest[3];
	word_t e = message_digest[4];
	word_t f = message_digest[5];
	word_t g = message_digest[6];
	word_t h = message_digest[7];

	for(std::size_t i = 0; i < 64; ++i){
		word_t temp1 = h + sigma1(e) + ch(e, f, g) + add_constant[i] + w[i];
		word_t temp2 = sigma0(a) + maj(a, b, c);
		h = g;
		g = f;
		f = e;
		e = mask_32bit(d + temp1);
		d = c;
		c = b;
		b = a;
		a = mask_32bit(temp1 + temp2);
	}
	message_digest[0] += a;
	message_digest[1] += b;
	message_digest[2] += c;
	message_digest[3] += d;
	message_digest[4] += e;
	message_digest[5] += f;
	message_digest[6] += g;
	message_digest[7] += h;
	for(std::size_t i = 0; i < 8; ++i){
		message_digest[i] = mask_32bit(message_digest[i]);
	}
}

} // namespace detail

class hash256_one_by_one
{
public:
	hash256_one_by_one(){
		std::copy(detail::initial_message_digest, detail::initial_message_digest+8, message_digest_state_);
		buffer_.reserve(64);
        datlen_ = 0;
	}

	template<typename InIter>
	void process(InIter first, InIter last){
        datlen_ += std::distance(first, last);
		std::copy(first, last, std::back_inserter(buffer_));
		std::vector<byte_t>::iterator p = buffer_.begin();
		while(p + 64 <= buffer_.end()){
			detail::process_chunk(p, p+64, message_digest_state_);
			p += 64;
		}
		buffer_.erase(buffer_.begin(), p);
	}

	void finish(){
		byte_t l_byte[8];
		size_t message_bit_length = datlen_ * 8;
		for(int i = 7; i >= 0; --i){
			l_byte[i] = detail::mask_8bit(static_cast<byte_t>(message_bit_length));
			message_bit_length >>= 8;
		}

		buffer_.push_back(0x80);

		if(buffer_.size() > 56){
			std::fill_n(std::back_inserter(buffer_), 64 - buffer_.size(), 0);
			detail::process_chunk(buffer_.begin(), buffer_.end(), message_digest_state_);
			buffer_.clear();
		}

		std::fill_n(std::back_inserter(buffer_), 56 - buffer_.size(), 0);
		buffer_.insert(buffer_.end(), l_byte, l_byte + 8);

		assert(buffer_.size() == 64);
		detail::process_chunk(buffer_.begin(), buffer_.end(), message_digest_state_);
	}

	template<typename OutIter>
	void get_hash_bytes(OutIter first, OutIter last) const{
		assert(std::distance(first, last) == k_digest_size);
		for(size_t i = 0; i < 8; ++i){
			for(int j = 3; j >= 0; --j){
				*first++ = detail::mask_8bit(static_cast<byte_t>(message_digest_state_[i] >> (j*8)));
			}
		}
	}

private:
	word_t message_digest_state_[8]; //A-H
	std::vector<byte_t> buffer_;
    size_t datlen_;
};

template<typename InIter, typename OutIter>
void hash256(InIter first, InIter last, OutIter first2, OutIter last2,
			 typename std::iterator_traits<InIter>::iterator_category* = NULL)
{
	hash256_one_by_one hasher;
	hasher.process(first, last);
	hasher.finish();
	hasher.get_hash_bytes(first2, last2);
}

template<typename InIter, typename OutIter>
void hash256(InIter first, InIter last, OutIter first2, OutIter last2,
			 std::input_iterator_tag*)
{
	std::vector<byte_t> buffer(PICOSHA2_BUFFER_SIZE_FOR_INPUT_ITERATOR);
	hash256_one_by_one hasher;
	while(first != last){
		int blk_size = 0;
		for(std::vector<byte_t>::iterator it = buffer.begin();
			it != buffer.end() && first != last; ++it, ++blk_size){
			*it = *first++;
		}
		hasher.process(buffer.begin(), buffer.begin()+blk_size);
	}
	hasher.finish();
	hasher.get_hash_bytes(first2, last2);
}

template<typename In, typename Out>
void hash256(const In& in, Out& out){
	hash256(in.begin(), in.end(), out.begin(), out.end());
}

template<typename InIter>
std::vector<unsigned char> hash256(InIter first, InIter last){
	std::vector<unsigned char> digest(k_digest_size);
	hash256(first, last, digest.begin(), digest.end());
	return digest;
}

template<typename In>
std::vector<unsigned char> hash256(const In& in){
	return hash256(in.begin(), in.end());
}

template<typename OutIter>
std::string get_hash_hex_string(OutIter first, OutIter last){
	std::stringstream ss;
	ss << std::hex << std::setfill('0');
	while(first != last){
		ss << std::setw(2) << static_cast<unsigned int>(*first++);
	}
	return ss.str();
}

template<typename In>
std::string get_hash_hex_string(const In& in){
	return get_hash_hex_string(in.begin(), in.end());
}

template<typename InIter>
void hash256_hex_string(InIter first, InIter last, std::string& hex_str)
{
    hex_str = get_hash_hex_string(hash256(first, last));
}

template<typename In>
void hash256_hex_string(const In& in, std::string& hex_str)
{
    hash256_hex_string(in.begin(), in.end(), hex_str);
}

template<typename InIter>
std::string hash256_hex_string(InIter first, InIter last)
{
    return get_hash_hex_string(hash256(first, last));
}

template<typename In>
std::string hash256_hex_string(const In& in)
{
    return hash256_hex_string(in.begin(), in.end());
}

} // namespace picosha2

#endif //PICOSHA2_H