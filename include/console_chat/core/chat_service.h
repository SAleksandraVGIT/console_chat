#pragma once

#include "user.h"
#include "base_chat.h"
#include "service_state.h"
#include "console_chat/storage/imanager.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace console_chat::core {

inline constexpr const char* GENERAL_CHAT_NAME = "GENERAL";

inline constexpr size_t MAX_PRIVATE_CHATS_PER_USER = 45;
inline constexpr size_t MAX_MESSAGE_LENGTH = 256;

class ChatService {
public:
    ChatService();
    explicit ChatService(storage::IManager& storageManager);

    bool Initialize();

    bool Register(std::string&& name, std::string&& login, std::string&& password);
    bool Authenticate(const std::string& login, const std::string& password);
    std::string GetUserNameByLogin(const std::string& login) const;

    std::vector<std::string> GetMyChats(const std::string& currentLogin) const;
    bool CreatePrivateChat(const std::string& currentLogin, std::string&& recipientLogin, std::string&& chatName);

    std::vector<Message> GetMessages(const std::string& currentLogin, const std::string& chatName) const;
    bool SendMessage(const std::string& currentLogin, const std::string& chatName, std::string&& text);

    std::vector<std::string> GetAllUserLogins() const;

    bool ImportState(ServiceState state);

private:
    User* FindUser(const std::string& login) const;
    BaseChat* FindChat(const std::string& name) const;

    bool PersistUser(const UserState& user);
    bool PersistChat(const ChatState& chat);
    bool PersistMessage(
        const std::string& chatName,
        const std::string& senderLogin,
        const Message& message);

private:
    std::unordered_map<std::string, std::unique_ptr<User>> m_users;
    std::unordered_map<std::string, std::unique_ptr<BaseChat>> m_chats;
    storage::IManager* m_storageManager = nullptr;
};

} // namespace console_chat::core
