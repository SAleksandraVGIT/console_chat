#pragma once

#include "chat_service.h"

#include <string>


namespace console_chat {

class Console {
public:
    explicit Console(ChatService& service)
        : m_service(service) {}

    int Run();

private:
    void UserMenu();

    void RegistrationFlow();
    void LoginFlow();

    void CreatePrivateChatFlow();

    void OpenChatFlow();
    void OpenGeneralChatFlow();
    void ChatSession(const std::string& chatName);

    void ShowMyChatsFlow() const;
    void ShowAllUsersFlow() const;

    void ShowMainMenu() const;
    void ShowUserMenu() const;

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
        SHOW_ALL_USERS = 5
    };

private:
    ChatService& m_service;
};

} // namespace console_chat
