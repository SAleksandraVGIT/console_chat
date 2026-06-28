#include "console_chat/core/chat_service.h"
#include "console_chat/network/tcp_socket.h"
#include "console_chat/storage/file_manager.h"
#include "console_chat/storage/mysql_manager.h"

#include "session.h"

#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {

constexpr int DEFAULT_PORT = 7777;
constexpr int BACKLOG = 10;
constexpr int MIN_PORT = 1024;
constexpr int MAX_PORT = 49151;
constexpr const char* DEFAULT_USERS_FILE = "data/users.db";
constexpr const char* DEFAULT_CHATS_FILE = "data/chats.db";
constexpr const char* DEFAULT_MYSQL_CONFIG = "config/mysql.conf";

enum class StorageType {
    File,
    MySQL
};

StorageType ParseStorageType(const std::string& value) {
    if (value == "file") {
        return StorageType::File;
    }
    if (value == "mysql") {
        return StorageType::MySQL;
    }
    throw std::runtime_error("Storage must be 'file' or 'mysql'.");
}

void PrintUsage() {
    std::cout
        << "Usage: chat_server [--port <number>] [--storage <file|mysql>] "
           "[--users-file <path>] [--chats-file <path>] "
           "[--mysql-config <path>] [--reset-state]\n";
}

} // namespace

int RunServer(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    bool resetState = false;
    StorageType storageType = StorageType::File;
    std::string usersFilePath = DEFAULT_USERS_FILE;
    std::string chatsFilePath = DEFAULT_CHATS_FILE;
    std::string mysqlConfigPath = DEFAULT_MYSQL_CONFIG;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
            continue;
        }
        if (arg == "--storage" && i + 1 < argc) {
            storageType = ParseStorageType(argv[++i]);
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
        if (arg == "--mysql-config" && i + 1 < argc) {
            mysqlConfigPath = argv[++i];
            continue;
        }
        if (arg == "--reset-state") {
            resetState = true;
            continue;
        }
        if (arg == "--help") {
            PrintUsage();
            return 0;
        }

        throw std::runtime_error("Unknown argument: " + arg);
    }

    if (port < MIN_PORT || port > MAX_PORT) {
        throw std::runtime_error("Port must be in range 1024..49151.");
    }

    std::unique_ptr<console_chat::storage::IManager> storageManager;
    if (storageType == StorageType::File) {
        storageManager = std::make_unique<console_chat::storage::FileManager>(
            usersFilePath,
            chatsFilePath);
    } else {
        console_chat::storage::MySQLConfig config;
        std::string error;
        if (!console_chat::storage::LoadMySQLConfig(mysqlConfigPath, config, error)) {
            throw std::runtime_error(error);
        }
        storageManager = std::make_unique<console_chat::storage::MySQLManager>(
            std::move(config));
    }

    if (resetState) {
        if (!storageManager->Reset()) {
            std::string error = "Failed to reset saved state.";
            if (const auto* mysql =
                    dynamic_cast<console_chat::storage::MySQLManager*>(storageManager.get());
                mysql && !mysql->GetLastError().empty())
            {
                error += " " + mysql->GetLastError();
            }
            throw std::runtime_error(error);
        }
        std::cout << "State reset requested. Starting with empty state.\n";
    }

    console_chat::core::ChatService service(*storageManager);
    if (!service.Initialize()) {
        if (storageType == StorageType::File && !resetState &&
            storageManager->Reset() && service.Initialize())
        {
            std::cout << "No valid saved state found. Starting with empty state.\n";
        } else {
            std::string error = "Failed to initialize persistent state.";
            if (const auto* mysql =
                    dynamic_cast<console_chat::storage::MySQLManager*>(storageManager.get());
                mysql && !mysql->GetLastError().empty())
            {
                error += " " + mysql->GetLastError();
            }
            throw std::runtime_error(error);
        }
    }

    std::cout << "Storage backend: "
              << (storageType == StorageType::File ? "file" : "mysql") << "\n";

    console_chat::network::TcpSocket serverSock;
    serverSock.BindAndListen(static_cast<uint16_t>(port), BACKLOG);
    std::cout << "Server is listening on port " << port << "\n";

    std::mutex serviceMutex;
    while (true) {
        auto clientSock = serverSock.Accept();
        if (!clientSock.IsValid()) {
            continue;
        }

        std::thread([client = std::move(clientSock), &service, &serviceMutex]() mutable {
            console_chat::server::HandleClientSession(
                std::move(client),
                service,
                serviceMutex);
        }).detach();
    }
}

int main(int argc, char* argv[]) {
    try {
        return RunServer(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "Server error: " << error.what() << '\n';
        return 1;
    }
}
