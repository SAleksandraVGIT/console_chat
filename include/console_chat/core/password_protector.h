#pragma once

#include <string>
#include <string_view>


namespace console_chat::core {

class PasswordProtector {
public:
    static std::string Hash(std::string_view password);
    static bool Verify(std::string_view password, std::string_view expectedHash);
    static bool IsHash(std::string_view value);

private:
    static constexpr std::string_view HASH_PREFIX = "h1$";
};

} // namespace console_chat::core
