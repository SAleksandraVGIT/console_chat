#include "console_chat/client/client_config.h"

#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace console_chat::client {
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

bool LoadClientConfig(const std::string& path, ClientConfig& config, std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input) {
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        if (exists || ec) {
            error = "Failed to open client config: " + path;
            return false;
        }
        config = {};
        error = "Client config not found: " + path;
        return true;
    }

    ClientConfig loaded;
    bool configured = false;
    std::string line;
    while (std::getline(input, line)) {
        const auto trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }
        const auto separator = trimmed.find('=');
        if (separator == std::string_view::npos ||
            Trim(trimmed.substr(0, separator)) != "refresh_interval_ms") {
            error = "Invalid client config line: " + line;
            return false;
        }
        if (configured) {
            error = "Duplicate client config key: refresh_interval_ms";
            return false;
        }
        configured = true;
        const auto value = Trim(trimmed.substr(separator + 1));
        int milliseconds = 0;
        const auto [end, result] = std::from_chars(value.data(), value.data() + value.size(), milliseconds);
        if (result != std::errc{} || end != value.data() + value.size() ||
            milliseconds < 100 || milliseconds > 60000) {
            error = "refresh_interval_ms must be in range 100..60000";
            return false;
        }
        loaded.RefreshInterval = std::chrono::milliseconds{milliseconds};
    }
    if (input.bad()) {
        error = "Failed to read client config: " + path;
        return false;
    }
    config = loaded;
    return true;
}

} // namespace console_chat::client
