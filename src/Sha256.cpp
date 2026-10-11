// Sha256.cpp - written for clarity rather than speed. A production system
// would use a vetted library.

#include "Sha256.h"
#include <sstream>
#include <iomanip>
#include <cstring>

namespace crypto {

namespace {

// First 32 bits of the fractional parts of the cube roots of the first 64
// primes. These are the round constants from FIPS 180-4 section 4.2.2.
const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

inline uint32_t rotr(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

constexpr size_t BLOCK_BYTES  = 64;
constexpr size_t DIGEST_BYTES = 32;

}  // namespace

std::vector<unsigned char> sha256(const std::vector<unsigned char>& data) {
    // Initial hash values: first 32 bits of the fractional parts of the
    // square roots of the first eight primes.
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    // Padding: a single 1 bit, then zeros, then the length in bits as a
    // 64-bit big-endian value, so the total is a multiple of 64 bytes.
    std::vector<unsigned char> msg = data;
    const uint64_t bitLength = static_cast<uint64_t>(data.size()) * 8;

    msg.push_back(0x80);
    while (msg.size() % BLOCK_BYTES != 56) msg.push_back(0x00);
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<unsigned char>((bitLength >> (i * 8)) & 0xff));
    }

    for (size_t offset = 0; offset < msg.size(); offset += BLOCK_BYTES) {
        uint32_t w[64];

        // First 16 words come straight from the block, big-endian.
        for (int i = 0; i < 16; ++i) {
            const size_t j = offset + static_cast<size_t>(i) * 4;
            w[i] = (static_cast<uint32_t>(msg[j])     << 24) |
                   (static_cast<uint32_t>(msg[j + 1]) << 16) |
                   (static_cast<uint32_t>(msg[j + 2]) <<  8) |
                   (static_cast<uint32_t>(msg[j + 3]));
        }
        // The remaining 48 are derived, which is what spreads each input bit
        // across the whole schedule.
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2],  19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; ++i) {
            const uint32_t S1    = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const uint32_t ch    = (e & f) ^ ((~e) & g);
            const uint32_t temp1 = hh + S1 + ch + K[i] + w[i];
            const uint32_t S0    = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const uint32_t maj   = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t temp2 = S0 + maj;

            hh = g; g = f; f = e;
            e  = d + temp1;
            d  = c; c = b; b = a;
            a  = temp1 + temp2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    std::vector<unsigned char> digest(DIGEST_BYTES);
    for (int i = 0; i < 8; ++i) {
        digest[i * 4 + 0] = static_cast<unsigned char>((h[i] >> 24) & 0xff);
        digest[i * 4 + 1] = static_cast<unsigned char>((h[i] >> 16) & 0xff);
        digest[i * 4 + 2] = static_cast<unsigned char>((h[i] >>  8) & 0xff);
        digest[i * 4 + 3] = static_cast<unsigned char>( h[i]        & 0xff);
    }
    return digest;
}

std::vector<unsigned char> sha256(const std::string& data) {
    return sha256(std::vector<unsigned char>(data.begin(), data.end()));
}

// HMAC-SHA256, RFC 2104. The two padded passes close the length-extension
// weakness a plain hash of key-then-message would have.
std::vector<unsigned char> hmacSha256(const std::vector<unsigned char>& key,
                                      const std::vector<unsigned char>& message) {
    std::vector<unsigned char> block(BLOCK_BYTES, 0x00);

    if (key.size() > BLOCK_BYTES) {
        std::vector<unsigned char> shortened = sha256(key);
        std::copy(shortened.begin(), shortened.end(), block.begin());
    } else {
        std::copy(key.begin(), key.end(), block.begin());
    }

    std::vector<unsigned char> inner(BLOCK_BYTES), outer(BLOCK_BYTES);
    for (size_t i = 0; i < BLOCK_BYTES; ++i) {
        inner[i] = static_cast<unsigned char>(block[i] ^ 0x36);
        outer[i] = static_cast<unsigned char>(block[i] ^ 0x5c);
    }

    inner.insert(inner.end(), message.begin(), message.end());
    std::vector<unsigned char> innerDigest = sha256(inner);

    outer.insert(outer.end(), innerDigest.begin(), innerDigest.end());
    return sha256(outer);
}

// PBKDF2-HMAC-SHA256, RFC 8018 section 5.2. Each block is U1 xor ... xor Uc,
// each U being HMAC over the previous. The chain is sequential, which is where
// the work factor comes from.
std::vector<unsigned char> pbkdf2HmacSha256(const std::string& password,
                                            const std::vector<unsigned char>& salt,
                                            int iterations,
                                            size_t length) {
    const std::vector<unsigned char> key(password.begin(), password.end());
    std::vector<unsigned char> output;
    output.reserve(length);

    uint32_t blockIndex = 1;
    while (output.size() < length) {
        // U1 = HMAC(password, salt || INT_BE(blockIndex))
        std::vector<unsigned char> input = salt;
        input.push_back(static_cast<unsigned char>((blockIndex >> 24) & 0xff));
        input.push_back(static_cast<unsigned char>((blockIndex >> 16) & 0xff));
        input.push_back(static_cast<unsigned char>((blockIndex >>  8) & 0xff));
        input.push_back(static_cast<unsigned char>( blockIndex        & 0xff));

        std::vector<unsigned char> u = hmacSha256(key, input);
        std::vector<unsigned char> accumulator = u;

        for (int i = 1; i < iterations; ++i) {
            u = hmacSha256(key, u);
            for (size_t j = 0; j < accumulator.size(); ++j) {
                accumulator[j] = static_cast<unsigned char>(accumulator[j] ^ u[j]);
            }
        }

        const size_t take = std::min(accumulator.size(), length - output.size());
        output.insert(output.end(), accumulator.begin(),
                      accumulator.begin() + static_cast<long>(take));
        ++blockIndex;
    }
    return output;
}

std::string toHex(const std::vector<unsigned char>& bytes) {
    std::ostringstream out;
    for (unsigned char b : bytes) {
        out << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(b);
    }
    return out.str();
}

std::vector<unsigned char> fromHex(const std::string& hex) {
    std::vector<unsigned char> bytes;
    if (hex.size() % 2 != 0) return bytes;
    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned int value = 0;
        std::istringstream(hex.substr(i, 2)) >> std::hex >> value;
        bytes.push_back(static_cast<unsigned char>(value));
    }
    return bytes;
}

}  // namespace crypto
