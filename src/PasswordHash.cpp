// PasswordHash.cpp

#include "PasswordHash.h"
#include "Sha256.h"
#include <random>
#include <sstream>
#include <vector>

namespace pwhash {

namespace {

constexpr size_t DIGEST_BYTES = 32;      // one SHA-256 block, no stretching
const char* const PREFIX = "pbkdf2_sha256";

std::vector<std::string> splitRecord(const std::string& stored) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : stored) {
        if (c == '$') {
            parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    parts.push_back(current);
    return parts;
}

}  // namespace

std::string deriveDigest(const std::string& password,
                         const std::string& saltHex,
                         int iterations) {
    // The salt is fed to the KDF as the bytes it encodes, not as its hex
    // text, so the stored form and the derivation agree on what the salt is.
    const std::vector<unsigned char> salt = crypto::fromHex(saltHex);
    return crypto::toHex(
        crypto::pbkdf2HmacSha256(password, salt, iterations, DIGEST_BYTES));
}

std::string hashPassword(const std::string& password, int iterations) {
    if (iterations <= 0) iterations = DEFAULT_ITERATIONS;

    // random_device is the platform entropy source. A salt that is guessable
    // is a salt that does not work, because the whole job of the salt is to
    // make a precomputed table useless.
    std::random_device rd;
    std::uniform_int_distribution<int> byteDist(0, 255);

    std::vector<unsigned char> salt(SALT_BYTES);
    for (size_t i = 0; i < SALT_BYTES; ++i) {
        salt[i] = static_cast<unsigned char>(byteDist(rd));
    }
    const std::string saltHex = crypto::toHex(salt);

    std::ostringstream record;
    record << PREFIX << "$" << iterations << "$" << saltHex << "$"
           << deriveDigest(password, saltHex, iterations);
    return record.str();
}

bool looksHashed(const std::string& stored) {
    return splitRecord(stored).size() == 4 &&
           stored.rfind(std::string(PREFIX) + "$", 0) == 0;
}

bool verifyPassword(const std::string& password, const std::string& stored) {
    if (!looksHashed(stored)) return false;          // fail closed

    const std::vector<std::string> parts = splitRecord(stored);
    const std::string& saltHex = parts[2];
    const std::string& digest  = parts[3];
    if (saltHex.empty() || digest.empty()) return false;

    int iterations = 0;
    std::istringstream iss(parts[1]);
    if (!(iss >> iterations) || iterations <= 0) return false;

    // Guard against a record claiming an absurd factor, which would turn a
    // sign-in attempt into an unbounded wait.
    if (iterations > 10 * DEFAULT_ITERATIONS) return false;

    const std::string expected = deriveDigest(password, saltHex, iterations);

    // Compare every character rather than stopping at the first difference.
    // A short-circuit comparison leaks, through its running time, how much of
    // a guess was correct.
    if (expected.size() != digest.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < expected.size(); ++i) {
        diff |= static_cast<unsigned char>(expected[i] ^ digest[i]);
    }
    return diff == 0;
}

}  // namespace pwhash
