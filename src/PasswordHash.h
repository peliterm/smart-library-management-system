// PasswordHash.h - PBKDF2-HMAC-SHA256 with a random 16-byte salt.
//
// Default 600,000 iterations; seeded demo accounts use 10,000 so the demo and
// test suite stay fast. The iteration count is stored in each record, so both
// verify correctly and raising the default later does not lock anyone out.
//
// Educational implementation. Argon2id is the preferred algorithm; the
// rationale and limitations are in the technical report.

#ifndef PASSWORD_HASH_H
#define PASSWORD_HASH_H

#include <string>
#include <cstddef>

namespace pwhash {

// OWASP's figure for PBKDF2-HMAC-SHA256. Roughly 1.5 seconds per derivation
// in an optimized build on this machine, which is the cost an attacker pays
// per guess and the cost a patron pays once at sign-in.
constexpr int DEFAULT_ITERATIONS = 600000;

// A deliberately reduced factor for seeded sample accounts only.
//
// The demo creates five accounts at startup and the test suite creates many
// more. At the default factor that is seconds of waiting before a screen
// appears, which would make the walkthrough unusable and the suite slow
// enough to stop being run. The iteration count is stored inside each record,
// so a sample account hashed at this factor and a real account hashed at the
// default both verify correctly. No account a person chose a password for
// uses this value.
constexpr int DEMO_ITERATIONS = 10000;

// Salt length in bytes before hex encoding. 16 bytes is well past the point
// where a precomputed table is feasible.
constexpr size_t SALT_BYTES = 16;

// Produces a storable record:
//   pbkdf2_sha256$<iterations>$<salt-hex>$<digest-hex>
// The algorithm name and work factor travel with the digest, so raising the
// factor later does not lock out accounts stored under the old one, and a
// future change of algorithm can be detected rather than guessed.
std::string hashPassword(const std::string& password,
                         int iterations = DEFAULT_ITERATIONS);

// Recomputes the digest using the salt and iteration count recorded inside
// `stored`. Returns false for a malformed or empty record rather than
// throwing, so a corrupted field fails closed.
bool verifyPassword(const std::string& password, const std::string& stored);

// True when the string is a record this module produced. Accounts created
// before hashing existed hold something else, and sign-in has to tell them
// apart rather than letting an unrecognised field through.
bool looksHashed(const std::string& stored);

// Exposed for testing: the derived digest on its own, hex encoded.
std::string deriveDigest(const std::string& password,
                         const std::string& saltHex,
                         int iterations);

}  // namespace pwhash

#endif // PASSWORD_HASH_H
