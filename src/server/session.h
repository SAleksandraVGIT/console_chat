#pragma once

#include "console_chat/core/chat_service.h"
#include "console_chat/network/tcp_socket.h"

#include <mutex>
#include <string>

namespace console_chat::server {

void HandleClientSession(
    network::TcpSocket client,
    core::ChatService& service,
    std::mutex& serviceMutex);

} // namespace console_chat::server
