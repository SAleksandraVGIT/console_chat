#include "console_chat/core/password_protector.h"

#include <cstdint>
#include <iomanip>
#include <sstream>


namespace console_chat::core {

std::string PasswordProtector::Hash(std::string_view password) {
    constexpr std::uint64_t FNV_OFFSET_BASIS = 14695981039346656037ull;
    constexpr std::uint64_t FNV_PRIME = 1099511628211ull;
    constexpr std::string_view PASSWORD_SALT = "console_chat_local_salt_v1";

    std::uint64_t hash = FNV_OFFSET_BASIS;

    for (unsigned char c : PASSWORD_SALT) {
        hash ^= c;
        hash *= FNV_PRIME;
    }

    for (unsigned char c : password) {
        hash ^= c;
        hash *= FNV_PRIME;
    }

    std::ostringstream oss;
    oss << HASH_PREFIX
        << std::hex
        << std::nouppercase
        << std::setw(16)
        << std::setfill('0')
        << hash;

    return oss.str();
}

bool PasswordProtector::Verify(std::string_view password, std::string_view expectedHash) {
    return Hash(password) == expectedHash;
}

bool PasswordProtector::IsHash(std::string_view value) {
    return value.starts_with(HASH_PREFIX);
}

} // namespace console_chat::core
