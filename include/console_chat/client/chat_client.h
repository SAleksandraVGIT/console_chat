#pragma once

#include "console_chat/core/base_chat.h"
#include "console_chat/network/tcp_socket.h"

#include <string>
#include <vector>

namespace console_chat::client {

struct AdminUserInfo {
    std::string Login;
    std::string Name;
    std::string BanStatus;
    std::string BannedUntilEpoch;
    std::string ServerNowEpoch;
};

struct AdminChatInfo {
    std::string Name;
    std::string Type;
    std::string FirstUserLogin;
    std::string SecondUserLogin;
};

struct CreatePrivateChatResult {
    bool Success = false;
    std::string ExistingChatName;
};

struct AuthResult {
    bool Success = false;
    std::string Error;
    std::string BanStatus;
    std::string BannedUntilEpoch;
    std::string ServerNowEpoch;
};

class ChatClient {
public:
    ChatClient(const std::string& host, const int port);
    ~ChatClient();

    bool Register(std::string&& name, std::string&& login, std::string&& password);
    bool Authenticate(const std::string& login, const std::string& password);
    AuthResult AuthenticateDetailed(const std::string& login, const std::string& password);
    void Logout();
    bool DeleteAccount(const std::string& confirmationLogin);

    bool IsAuthenticated() const;

    std::string GetCurrentUserLogin() const;
    std::string GetCurrentUserName() const;

    std::vector<std::string> GetMyChats(bool background = false) const;
    bool CreatePrivateChat(std::string&& recipientLogin, std::string&& chatName);
    CreatePrivateChatResult CreatePrivateChatDetailed(
        std::string&& recipientLogin,
        std::string&& chatName);

    std::vector<core::Message> GetMessages(const std::string& chatName, bool background = false) const;
    bool SendMessage(const std::string& chatName, std::string&& text);

    std::vector<std::string> GetAllUserLogins(bool background = false) const;
    void NotifyActivity() const;

    bool AdminLogin(const std::string& login, const std::string& password);
    std::vector<AdminUserInfo> AdminGetUsers(bool background = false) const;
    std::vector<AdminChatInfo> AdminGetChats(bool background = false) const;
    bool AdminCreatePrivateChat(std::string&& recipientLogin, std::string&& chatName);
    CreatePrivateChatResult AdminCreatePrivateChatDetailed(
        std::string&& recipientLogin,
        std::string&& chatName);
    std::vector<core::Message> AdminGetMessages(const std::string& chatName, bool background = false) const;
    bool AdminSendMessageToChat(const std::string& chatName, std::string&& text);
    bool AdminSendGeneral(std::string&& text);
    bool AdminKickUser(const std::string& login);
    bool AdminBanUser(const std::string& login, const std::string& period);
    bool AdminUnbanUser(const std::string& login);
    int AdminDeleteForeverBannedUsers();

private:
    std::vector<std::string> Request(const std::vector<std::string>& parts, bool background = false) const;
    static std::vector<std::string> Split(const std::string& line, const char delim);

private:
    mutable network::TcpSocket m_socket;
};

} // namespace console_chat::client
