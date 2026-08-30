#pragma once

#include "console_chat/core/chat_service.h"

#include <chrono>
#include <string>

namespace console_chat::server {

struct ServerConfig {
    core::ServiceLimits Limits;
    std::chrono::seconds ClientIdleTimeout{std::chrono::minutes{15}};
};

bool LoadServerConfig(
    const std::string& filePath,
    ServerConfig& config,
    std::string& error);

} // namespace console_chat::server
