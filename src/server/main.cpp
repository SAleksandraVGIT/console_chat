#include "console_chat/core/chat_service.h"
#include "console_chat/network/tcp_socket.h"

#include "session.h"

#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

namespace fs = std::filesystem;

constexpr int DEFAULT_PORT = 7777;
constexpr int BACKLOG = 10;
constexpr int MIN_PORT = 1024;
constexpr int MAX_PORT = 49151;
constexpr const char* DEFAULT_STATE_DIR = "data";
constexpr const char* DEFAULT_USERS_FILE = "data/users.db";
constexpr const char* DEFAULT_CHATS_FILE = "data/chats.db";

} // namespace

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    bool resetState = false;
    std::string usersFilePath = DEFAULT_USERS_FILE;
    std::string chatsFilePath = DEFAULT_CHATS_FILE;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
            continue;
        }
        if (arg == "--users-file" && i + 1 < argc) {
            usersFilePath = argv[++i];
            continue;
        }
        if (arg == "--chats-file" && i + 1 < argc) {
            chatsFilePath = argv[++i];
            continue;
        }
        if (arg == "--reset-state") {
            resetState = true;
            continue;
        }
        if (arg == "--help") {
            std::cout << "Usage: chat_server [--port <number>] [--users-file <path>] "
                         "[--chats-file <path>] [--reset-state]\n";
            return 0;
        }

        throw std::runtime_error("Unknown argument: " + arg);
    }

    if (port < MIN_PORT || port > MAX_PORT) {
        throw std::runtime_error("Port must be in range 1024..49151.");
    }

    console_chat::core::ChatService service;
    fs::create_directories(fs::path(DEFAULT_STATE_DIR));
    const auto usersDir = fs::path(usersFilePath).parent_path();
    const auto chatsDir = fs::path(chatsFilePath).parent_path();
    if (!usersDir.empty()) {
        fs::create_directories(usersDir);
    }
    if (!chatsDir.empty()) {
        fs::create_directories(chatsDir);
    }

    if (resetState) {
        std::error_code ec;
        fs::remove(usersFilePath, ec);
        fs::remove(chatsFilePath, ec);
        std::cout << "State reset requested. Starting with empty state.\n";
    } else if (!service.LoadState(usersFilePath, chatsFilePath)) {
        std::cout << "No valid saved state found. Starting with empty state.\n";
    }

    if (!service.SaveState(usersFilePath, chatsFilePath)) {
        throw std::runtime_error("Failed to initialize state files.");
    }

    console_chat::network::TcpSocket serverSock;
    serverSock.BindAndListen(static_cast<uint16_t>(port), BACKLOG);
    std::cout << "Server is listening on port " << port << "\n";

    std::mutex serviceMutex;
    while (true) {
        auto clientSock = serverSock.Accept();
        if (!clientSock.IsValid()) {
            continue;
        }
        std::thread([client = std::move(clientSock), &service, &serviceMutex, usersFilePath, chatsFilePath]() mutable {
            console_chat::server::HandleClientSession(
                std::move(client),
                service,
                serviceMutex,
                usersFilePath,
                chatsFilePath);
        }).detach();
    }
}
