#pragma once

#include <chrono>
#include <string>

namespace console_chat::client {

struct ClientConfig {
    std::chrono::milliseconds RefreshInterval{3000};
};

bool LoadClientConfig(const std::string& path, ClientConfig& config, std::string& error);

} // namespace console_chat::client
