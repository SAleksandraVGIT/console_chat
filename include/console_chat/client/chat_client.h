#pragma once

#include "console_chat/core/base_chat.h"
#include "console_chat/network/tcp_socket.h"

#include <string>
#include <vector>

namespace console_chat::client {

class ChatClient {
public:
    ChatClient(const std::string& host, const int port);
    ~ChatClient();

    bool Register(std::string&& name, std::string&& login, std::string&& password);
    bool Authenticate(const std::string& login, const std::string& password);
    void Logout();

    bool IsAuthenticated() const;
    std::string GetCurrentUserName() const;

    std::vector<std::string> GetMyChats() const;
    bool CreatePrivateChat(std::string&& recipientLogin, std::string&& chatName);

    std::vector<core::Message> GetMessages(const std::string& chatName) const;
    bool SendMessage(const std::string& chatName, std::string&& text);

    std::vector<std::string> GetAllUserLogins() const;

private:
    std::vector<std::string> Request(const std::vector<std::string>& parts) const;
    static std::vector<std::string> Split(const std::string& line, const char delim);

private:
    mutable network::TcpSocket m_socket;
};

} // namespace console_chat::client
