#include "admin_config.h"

#include <cctype>
#include <fstream>
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

} // namespace

bool LoadAdminCredentials(
    const std::string& filePath,
    AdminCredentials& credentials,
    std::string& error)
{
    std::ifstream input(filePath);
    if (!input) {
        credentials = {};
        error = "Admin config not found: " + filePath;
        return true;
    }

    AdminCredentials loaded;
    std::unordered_set<std::string> keys;
    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;
        const std::string_view trimmedLine = Trim(line);
        if (trimmedLine.empty() || trimmedLine.front() == '#') {
            continue;
        }

        const auto separator = trimmedLine.find('=');
        if (separator == std::string_view::npos) {
            error = "Invalid admin config line " + std::to_string(lineNumber);
            return false;
        }

        const std::string key(Trim(trimmedLine.substr(0, separator)));
        const std::string value(Trim(trimmedLine.substr(separator + 1)));

        if (!keys.insert(key).second) {
            error = "Duplicate admin config key: " + key;
            return false;
        }

        if (key == "login" || key == "name") {
            loaded.Login = value;
        } else if (key == "password") {
            loaded.Password = value;
        } else {
            error = "Unknown admin config key: " + key;
            return false;
        }
    }

    if (loaded.Login.empty() || loaded.Password.empty()) {
        error = "Admin config must contain login and password";
        return false;
    }

    loaded.Enabled = true;
    credentials = std::move(loaded);
    error.clear();
    return true;
}

} // namespace console_chat::server
