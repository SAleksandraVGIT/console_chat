#include "app.h"
#include "theme.h"
#include "widgets/main_window.h"
#include "console_chat/client/client_config.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <iostream>

namespace console_chat::qt {

int RunApplication(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName("Console Chat");
    QApplication::setOrganizationName("console_chat");

    ApplyTheme();

    QCommandLineParser parser;
    parser.setApplicationDescription("Qt chat client");
    parser.addHelpOption();
    parser.addOptions({
        {"host", "Server IPv4 address", "address", "127.0.0.1"},
        {"port", "Server TCP port", "port", "7777"},
        {"admin", "Select administrator login"},
        {"client-config", "Client configuration file", "path", "config/client.conf"},
        {"ui", "Client interface (qt)", "type", "qt"}
    });

    parser.process(application);

    if (parser.value("ui") != "qt") {
        std::cerr << "chat_gui supports --ui qt. Use console_chat for the console interface.\n";
        return 1;
    }

    bool validPort = false;
    const int port = parser.value("port").toInt(&validPort);
    if (!validPort || port < 1024 || port > 49151) {
        std::cerr << "Port must be in range 1024..49151.\n";
        return 1;
    }

    client::ClientConfig config;
    std::string error;
    if (!client::LoadClientConfig(parser.value("client-config").toStdString(), config, error)) {
        QMessageBox::critical(nullptr, "Client configuration", QString::fromStdString(error));
        return 1;
    }

    if (!error.empty()) {
        std::cerr << error << ". Using default refresh interval (3000 ms).\n";
    }

    LoginRequest defaults;
    defaults.host = parser.value("host");
    defaults.port = port;
    defaults.admin = parser.isSet("admin");

    MainWindow window(defaults, static_cast<int>(config.RefreshInterval.count()));
    window.show();
    return application.exec();
}

} // namespace console_chat::qt
