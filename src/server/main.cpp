#include "console_chat/core/chat_service.h"
#include "console_chat/network/tcp_socket.h"

#include "session.h"

#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

constexpr int DEFAULT_PORT = 7777;
constexpr int BACKLOG = 10;
constexpr int MIN_PORT = 1024;
constexpr int MAX_PORT = 49151;

} // namespace

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
            continue;
        }
        if (arg == "--help") {
            std::cout << "Usage: chat_server [--port <number>]\n";
            return 0;
        }

        throw std::runtime_error("Unknown argument: " + arg);
    }

    if (port < MIN_PORT || port > MAX_PORT) {
        throw std::runtime_error("Port must be in range 1024..49151.");
    }

    console_chat::network::TcpSocket serverSock;
    serverSock.BindAndListen(static_cast<uint16_t>(port), BACKLOG);

    std::cout << "Server is listening on port " << port << "\n";

    console_chat::core::ChatService service;
    std::mutex serviceMutex;
    while (true) {
        auto clientSock = serverSock.Accept();
        if (!clientSock.IsValid()) {
            continue;
        }
        std::thread([client = std::move(clientSock), &service, &serviceMutex]() mutable {
            console_chat::server::HandleClientSession(std::move(client), service, serviceMutex);
        }).detach();
    }
}
