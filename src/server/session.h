#pragma once

#include "console_chat/core/chat_service.h"
#include "console_chat/network/tcp_socket.h"
#include "request_router.h"

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace console_chat::server {

class SessionRegistry final : public SessionController {
public:
    void Register(const std::string& login, network::TcpSocket& socket);
    void Unregister(const std::string& login, network::TcpSocket& socket);
    bool KickUser(const std::string& login) override;

private:
    std::mutex m_mutex;
    std::unordered_map<std::string, std::vector<network::TcpSocket*>> m_sessions;
};

void HandleClientSession(
    network::TcpSocket client,
    core::ChatService& service,
    std::mutex& serviceMutex,
    const AdminCredentials& adminCredentials = AdminCredentials{},
    SessionRegistry* sessionRegistry = nullptr,
    std::chrono::seconds clientIdleTimeout = std::chrono::minutes{15});

} // namespace console_chat::server
