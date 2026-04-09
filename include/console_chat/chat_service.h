#pragma once

#include "user.h"
#include "base_chat.h"

#include <memory>
#include <string>
#include <unordered_map>
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
        m_currentUserLogin.clear();
    }

    inline bool IsAuthenticated() const {
        return !m_currentUserLogin.empty();
    }

    inline std::string GetCurrentUserName() const {
        if (m_currentUserLogin.empty()) {
            return std::string{};
        }

        const auto it = m_users.find(m_currentUserLogin);
        return it != m_users.end() ? std::get<std::unique_ptr<console_chat::User>>(*it)->GetName() : std::string{};
    }

private:
    User* FindUser(const std::string& login) const;
    BaseChat* FindChat(const std::string& name) const;

private:
    std::unordered_map<std::string, std::unique_ptr<User>> m_users;
    std::unordered_map<std::string, std::unique_ptr<BaseChat>> m_chats;

    std::string m_currentUserLogin;
};

} // namespace console_chat
