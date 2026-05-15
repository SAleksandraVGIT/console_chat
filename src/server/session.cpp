#include "session.h"

#include "protocol.h"
#include "request_router.h"

#include <string>
#include <vector>

namespace console_chat::server {

void HandleClientSession(
    network::TcpSocket client,
    core::ChatService& service,
    std::mutex& serviceMutex,
    const std::string& usersFilePath,
    const std::string& chatsFilePath)
{
    std::string currentLogin;
    std::string line;

    while (client.RecvLine(line)) {
        const auto req = Split(line, '\t');
        std::vector<std::string> resp;

        {
            std::lock_guard<std::mutex> lock(serviceMutex);
            resp = HandleRequest(req, service, currentLogin, usersFilePath, chatsFilePath);
        }

        if (!client.SendLine(Join(resp, '\t'))) {
            break;
        }
    }
}

} // namespace console_chat::server
