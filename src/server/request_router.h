#pragma once

#include "console_chat/core/chat_service.h"

#include <string>
#include <vector>

namespace console_chat::server {

struct AdminCredentials {
    std::string Login;
    std::string Password;
    bool Enabled = false;
};

struct RequestContext {
    std::string currentLogin;
    bool isAdmin = false;
};

class SessionController {
public:
    virtual ~SessionController() = default;
    virtual bool KickUser(const std::string& login) = 0;
};

std::vector<std::string> HandleRequest(
    const std::vector<std::string>& req,
    core::ChatService& service,
    RequestContext& context,
    const AdminCredentials& adminCredentials,
    SessionController* sessionController = nullptr);

std::vector<std::string> HandleRequest(
    const std::vector<std::string>& req,
    core::ChatService& service,
    std::string& currentLogin);

} // namespace console_chat::server
