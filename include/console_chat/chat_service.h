#pragma once

#include "user.h"
#include "base_chat.h"

#include <memory>
#include <string>
#include <vector>


namespace console_chat {

static const char* GENERAL_CHAT_NAME = "GENERAL";

class ChatService {
public:
    ChatService();

    bool Register(std::string&& name, std::string&& login, std::string&& password);
    bool Authenticate(const std::string& login, const std::string& password);

    std::vector<std::string> GetMyChats() const;
    bool CreatePrivateChat(std::string&& recipientLogin, std::string&& chatName);

    std::vector<Message> GetMessages(const std::string& chatName) const;
    bool SendMessage(const std::string& chatName, std::string&& text);

    std::vector<std::string> GetAllUserLogins() const;

    inline void Logout() {
        m_currentUser = nullptr;
    }

    inline bool IsAuthenticated() const {
        return m_currentUser != nullptr;
    }

    inline std::string GetCurrentUserName() const {
        return m_currentUser ? m_currentUser->GetName() : std::string{};
    }

private:
    User* FindUser(const std::string& login) const;
    BaseChat* FindChat(const std::string& name) const;

private:
    std::vector<std::unique_ptr<User>> m_users;
    std::vector<std::unique_ptr<BaseChat>> m_chats;

    User* m_currentUser = nullptr;
};

} // namespace console_chat
