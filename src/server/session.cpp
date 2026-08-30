#include "session.h"

#include "protocol.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

namespace console_chat::server {

namespace {

constexpr const char* ADMIN_DISCONNECT_RESPONSE = "ERR\tdisconnected by ADMIN";
constexpr const char* IDLE_TIMEOUT_RESPONSE = "ERR\tdisconnected by inactivity timeout";

} // namespace

void SessionRegistry::Register(const std::string& login, network::TcpSocket& socket) {
    if (login.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessions[login].push_back(&socket);
}

void SessionRegistry::Unregister(const std::string& login, network::TcpSocket& socket) {
    if (login.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_sessions.find(login);
    if (it == m_sessions.end()) {
        return;
    }

    auto& sockets = it->second;
    sockets.erase(
        std::remove(sockets.begin(), sockets.end(), &socket),
        sockets.end());

    if (sockets.empty()) {
        m_sessions.erase(it);
    }
}

bool SessionRegistry::KickUser(const std::string& login) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_sessions.find(login);
    if (it == m_sessions.end()) {
        return false;
    }

    for (auto* socket : it->second) {
        if (socket) {
            socket->SendLine(ADMIN_DISCONNECT_RESPONSE);
            socket->Close();
        }
    }

    return true;
}

void HandleClientSession(
    network::TcpSocket client,
    core::ChatService& service,
    std::mutex& serviceMutex,
    const AdminCredentials& adminCredentials,
    SessionRegistry* sessionRegistry,
    const std::chrono::seconds clientIdleTimeout)
{
    client.SetReceiveTimeout(clientIdleTimeout);

    RequestContext context;
    std::string line;

    while (client.RecvLine(line)) {
        const auto req = Split(line, '\t');
        std::vector<std::string> resp;
        const std::string previousLogin = context.currentLogin;

        {
            std::lock_guard<std::mutex> lock(serviceMutex);
            resp = HandleRequest(req, service, context, adminCredentials, sessionRegistry);
        }

        if (sessionRegistry && previousLogin != context.currentLogin) {
            sessionRegistry->Unregister(previousLogin, client);
            sessionRegistry->Register(context.currentLogin, client);
        }

        if (!client.SendLine(Join(resp, '\t'))) {
            break;
        }
    }

    if (client.WasLastReceiveTimedOut()) {
        client.SendLine(IDLE_TIMEOUT_RESPONSE);
    }

    if (sessionRegistry) {
        sessionRegistry->Unregister(context.currentLogin, client);
    }
}

} // namespace console_chat::server
