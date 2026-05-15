#include "console_chat/client/chat_console.h"
#include "console_chat/client/chat_client.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

constexpr int MIN_PORT = 1024;
constexpr int MAX_PORT = 49151;
constexpr int DEFAULT_PORT = 7777;
constexpr const char* DEFAULT_HOST = "127.0.0.1";

} // namespace

int main(int argc, char* argv[]) {
    try {
        std::string host = DEFAULT_HOST;
        int port = DEFAULT_PORT;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--host" && i + 1 < argc) {
                host = argv[++i];
                continue;
            }
            if (arg == "--port" && i + 1 < argc) {
                port = std::stoi(argv[++i]);
                continue;
            }
            if (arg == "--help") {
                std::cout << "Usage: console_chat [--host <ip>] [--port <number>]\n";
                return 0;
            }

            throw std::runtime_error("Unknown argument: " + arg);
        }

        if (port < MIN_PORT || port > MAX_PORT) {
            throw std::runtime_error("Port must be in range 1024..49151.");
        }

        console_chat::client::ChatClient client(host, port);
        console_chat::client::ChatConsole console(client);
        return console.Run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error." << std::endl;
        return 1;
    }
}
