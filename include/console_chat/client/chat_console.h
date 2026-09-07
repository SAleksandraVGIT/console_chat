#pragma once

#include "chat_client.h"
#include "client_config.h"

#include <functional>
#include <string>


namespace console_chat::client {

class ChatConsole {
public:
    explicit ChatConsole(ChatClient& service, const ClientConfig& config = {})
        : m_service(service), m_config(config)
    {}

    int Run();
    int RunAdmin();

private:
    void UserMenu();
    void AdminMenu();

    void RegistrationFlow();
    void LoginFlow();
    void AdminLoginFlow();

    void CreatePrivateChatFlow();
    bool DeleteAccountFlow();

    void OpenChatFlow();
    void OpenGeneralChatFlow();
    void ChatSession(const std::string& chatName);
    void LiveChatSession(const std::string& chatName, bool admin, bool canSend);
    void ShowLiveList(const std::function<std::string(bool)>& snapshot) const;

    void ShowMyChatsFlow() const;
    void ShowAllUsersFlow() const;

    void AdminShowUsersFlow() const;
    void AdminShowChatsFlow() const;
    void AdminCreatePrivateChatFlow();
    void AdminOpenPrivateChatFlow();
    void AdminOpenGeneralChatFlow();
    void AdminChatSession(const std::string& chatName, bool canSend);
    void AdminKickUserFlow();
    void AdminBanUserFlow();
    void AdminUnbanUserFlow();
    void AdminDeleteForeverBannedUsersFlow();

    void ShowMainMenu() const;
    void ShowUserMenu() const;
    void ShowAdminMainMenu() const;
    void ShowAdminMenu() const;

private:
    enum class ActionMainMenu : int {
        EXIT = 0,
        REGISTRATION = 1,
        LOGIN = 2
    };

    enum class ActionUserMenu : int {
        LOG_OUT = 0,
        SHOW_MY_CHATS = 1,
        CREATE_PRIVATE_CHAT = 2,
        OPEN_PRIVATE_CHAT = 3,
        OPEN_GENERAL_CHAT = 4,
        SHOW_ALL_USERS = 5,
        DELETE_ACCOUNT = 6
    };

    enum class ActionAdminMainMenu : int {
        EXIT = 0,
        LOGIN = 1
    };

    enum class ActionAdminMenu : int {
        LOG_OUT = 0,
        SHOW_USERS = 1,
        SHOW_CHATS = 2,
        CREATE_PRIVATE_CHAT = 3,
        OPEN_PRIVATE_CHAT = 4,
        OPEN_GENERAL_CHAT = 5,
        KICK_USER = 6,
        BAN_USER = 7,
        UNBAN_USER = 8,
        DELETE_FOREVER_BANNED_USERS = 9
    };

private:
    ChatClient& m_service;
    ClientConfig m_config;
};

} // namespace console_chat::client
