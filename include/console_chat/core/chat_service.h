#pragma once

#include "user.h"
#include "base_chat.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace console_chat::core {

static const char* GENERAL_CHAT_NAME = "GENERAL";

class ChatService {
public:
    ChatService();

    bool Register(std::string&& name, std::string&& login, std::string&& password);
    bool Authenticate(const std::string& login, const std::string& password);
    std::string GetUserNameByLogin(const std::string& login) const;

    std::vector<std::string> GetMyChats(const std::string& currentLogin) const;
    bool CreatePrivateChat(const std::string& currentLogin, std::string&& recipientLogin, std::string&& chatName);

    std::vector<Message> GetMessages(const std::string& currentLogin, const std::string& chatName) const;
    bool SendMessage(const std::string& currentLogin, const std::string& chatName, std::string&& text);

    std::vector<std::string> GetAllUserLogins() const;

private:
    User* FindUser(const std::string& login) const;
    BaseChat* FindChat(const std::string& name) const;

private:
    std::unordered_map<std::string, std::unique_ptr<User>> m_users;
    std::unordered_map<std::string, std::unique_ptr<BaseChat>> m_chats;
};

} // namespace console_chat::core
