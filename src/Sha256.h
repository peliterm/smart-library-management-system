// Sha256.h - SHA-256 (FIPS 180-4), HMAC-SHA256 (RFC 2104) and
// PBKDF2-HMAC-SHA256 (RFC 8018), implemented here because the project carries
// no external dependencies.
//
// Correctness is established by published test vectors in section 16 of the
// test driver, not by inspection. An unverified hash implementation that
// agrees with itself proves nothing.

#ifndef SHA256_H
#define SHA256_H

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace crypto {

// 32-byte digest of the input.
std::vector<unsigned char> sha256(const std::vector<unsigned char>& data);
std::vector<unsigned char> sha256(const std::string& data);

// Keyed hash. Keys longer than the 64-byte block are hashed first; shorter
// keys are zero-padded, both per RFC 2104.
std::vector<unsigned char> hmacSha256(const std::vector<unsigned char>& key,
                                      const std::vector<unsigned char>& message);

// Derives `length` bytes from the password and salt, iterating HMAC-SHA256
// `iterations` times per block. Each iteration's output is XORed into the
// accumulator, so the whole chain must be computed and cannot be shortcut.
std::vector<unsigned char> pbkdf2HmacSha256(const std::string& password,
                                            const std::vector<unsigned char>& salt,
                                            int iterations,
                                            size_t length);

// Lowercase hex, for storing and comparing digests as text.
std::string toHex(const std::vector<unsigned char>& bytes);
std::vector<unsigned char> fromHex(const std::string& hex);

}  // namespace crypto

#endif // SHA256_H
