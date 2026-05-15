#pragma once

#include "console_chat/core/chat_service.h"

#include <string>
#include <vector>

namespace console_chat::server {

std::vector<std::string> HandleRequest(
    const std::vector<std::string>& req,
    core::ChatService& service,
    std::string& currentLogin,
    const std::string& usersFilePath,
    const std::string& chatsFilePath);

} // namespace console_chat::server
