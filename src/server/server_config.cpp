#include "server_config.h"

#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace console_chat::server {

namespace {

std::string_view Trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }

    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }

    return value;
}

template <typename Integer>
bool ParseUnsigned(
    const std::string_view value,
    Integer& result,
    const std::string_view key,
    std::string& error)
{
    Integer parsed = 0;
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (ec != std::errc{} || ptr != value.data() + value.size()) {
        error = "Invalid numeric value for server config key: " + std::string(key);
        return false;
    }

    result = parsed;
    return true;
}

bool ValidatePositive(
    const std::size_t value,
    const std::string_view key,
    std::string& error)
{
    if (value == 0) {
        error = "Server config key must be greater than zero: " + std::string(key);
        return false;
    }

    return true;
}

} // namespace

bool LoadServerConfig(
    const std::string& filePath,
    ServerConfig& config,
    std::string& error)
{
    std::ifstream input(filePath);
    if (!input) {
        config = {};
        error = "Server config not found: " + filePath;
        return true;
    }

    ServerConfig loaded;
    std::unordered_set<std::string> keys;
    std::string line;
    std::size_t lineNumber = 0;
    bool timeoutConfigured = false;

    while (std::getline(input, line)) {
        ++lineNumber;
        const std::string_view trimmedLine = Trim(line);
        if (trimmedLine.empty() || trimmedLine.front() == '#') {
            continue;
        }

        const auto separator = trimmedLine.find('=');
        if (separator == std::string_view::npos) {
            error = "Invalid server config line " + std::to_string(lineNumber);
            return false;
        }

        const std::string key(Trim(trimmedLine.substr(0, separator)));
        const std::string_view value = Trim(trimmedLine.substr(separator + 1));
        if (key.empty()) {
            error = "Empty server config key at line " + std::to_string(lineNumber);
            return false;
        }

        if (!keys.insert(key).second) {
            error = "Duplicate server config key: " + key;
            return false;
        }

        if (key == "max_users") {
            if (!ParseUnsigned(value, loaded.Limits.MaxUsers, key, error)) {
                return false;
            }
        } else if (key == "client_timeout_minutes") {
            if (timeoutConfigured) {
                error = "Duplicate server config timeout key";
                return false;
            }
            timeoutConfigured = true;
            std::chrono::minutes::rep minutes = 0;
            if (!ParseUnsigned(value, minutes, key, error)) {
                return false;
            }
            if (minutes >
                std::numeric_limits<std::chrono::seconds::rep>::max() / 60)
            {
                error = "Server config client timeout is too large";
                return false;
            }
            loaded.ClientIdleTimeout = std::chrono::minutes{minutes};
        } else if (key == "client_timeout_seconds") {
            if (timeoutConfigured) {
                error = "Duplicate server config timeout key";
                return false;
            }
            timeoutConfigured = true;
            std::chrono::seconds::rep seconds = 0;
            if (!ParseUnsigned(value, seconds, key, error)) {
                return false;
            }
            loaded.ClientIdleTimeout = std::chrono::seconds{seconds};
        } else if (key == "max_chats") {
            if (!ParseUnsigned(value, loaded.Limits.MaxChats, key, error)) {
                return false;
            }
        } else if (key == "max_private_chats_per_user") {
            if (!ParseUnsigned(value, loaded.Limits.MaxPrivateChatsPerUser, key, error)) {
                return false;
            }
        } else if (key == "max_message_length") {
            if (!ParseUnsigned(value, loaded.Limits.MaxMessageLength, key, error)) {
                return false;
            }
        } else if (key == "max_messages_per_chat") {
            if (!ParseUnsigned(value, loaded.Limits.MaxMessagesPerChat, key, error)) {
                return false;
            }
        } else {
            error = "Unknown server config key: " + key;
            return false;
        }
    }

    if (input.bad()) {
        error = "Failed to read server config file: " + filePath;
        return false;
    }

    if (loaded.ClientIdleTimeout <= std::chrono::seconds::zero()) {
        error = "Server config key must be greater than zero: client_timeout";
        return false;
    }

    if (!ValidatePositive(loaded.Limits.MaxChats, "max_chats", error) ||
        !ValidatePositive(loaded.Limits.MaxPrivateChatsPerUser, "max_private_chats_per_user", error) ||
        !ValidatePositive(loaded.Limits.MaxMessageLength, "max_message_length", error) ||
        !ValidatePositive(loaded.Limits.MaxMessagesPerChat, "max_messages_per_chat", error))
    {
        return false;
    }

    config = std::move(loaded);
    error.clear();
    return true;
}

} // namespace console_chat::server
