#include "console_chat/client/chat_console.h"
#include "console_chat/client/chat_client.h"
#include "console_chat/client/client_config.h"
#ifdef CONSOLE_CHAT_WITH_QT
#include "app.h"
#endif

#include <clocale>
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
        bool adminMode = false;
        std::string configPath = "config/client.conf";
        std::string ui = "console";

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
            if (arg == "--admin") {
                adminMode = true;
                continue;
            }
            if (arg == "--ui" && i + 1 < argc) {
                ui = argv[++i];
                continue;
            }
            if (arg == "--client-config" && i + 1 < argc) {
                configPath = argv[++i];
                continue;
            }
            if (arg == "--help") {
                std::cout << "Usage: console_chat [--host <ip>] [--port <number>] [--admin] "
                             "[--client-config <path>] [--ui console|qt]\n";
                return 0;
            }

            throw std::runtime_error("Unknown argument: " + arg);
        }

        if (port < MIN_PORT || port > MAX_PORT) {
            throw std::runtime_error("Port must be in range 1024..49151.");
        }
        if (ui == "qt") {
#ifdef CONSOLE_CHAT_WITH_QT
            return console_chat::qt::RunApplication(argc, argv);
#else
            throw std::runtime_error("Qt UI is not built. Configure with -DBUILD_QT_CLIENT=ON.");
#endif
        }
        if (ui != "console") {
            throw std::runtime_error("UI must be console or qt.");
        }

        std::setlocale(LC_CTYPE, "");
        console_chat::client::ClientConfig config;
        std::string error;
        if (!console_chat::client::LoadClientConfig(configPath, config, error)) {
            throw std::runtime_error(error);
        }
        if (!error.empty()) {
            std::cerr << error << ". Using default refresh interval (3000 ms).\n";
        }
        console_chat::client::ChatClient client(host, port);
        console_chat::client::ChatConsole console(client, config);
        return adminMode ? console.RunAdmin() : console.Run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error." << std::endl;
        return 1;
    }
}
