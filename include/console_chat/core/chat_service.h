#pragma once

#include "user.h"
#include "base_chat.h"
#include "service_state.h"
#include "console_chat/storage/imanager.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>


namespace console_chat::core {

inline constexpr const char* GENERAL_CHAT_NAME = "GENERAL";
inline constexpr const char* ADMIN_MESSAGE_NAME = "ADMIN";
inline constexpr const char* ADMIN_SYSTEM_LOGIN = "__admin__";

inline constexpr size_t MAX_USERS = 0;
inline constexpr size_t MAX_CHATS_ON_SERVER = 991;
inline constexpr size_t MAX_PRIVATE_CHATS_PER_USER = 45;
inline constexpr size_t MAX_MESSAGE_LENGTH = 256;

struct ServiceLimits {
    size_t MaxUsers = MAX_USERS;
    size_t MaxChats = MAX_CHATS_ON_SERVER;
    size_t MaxPrivateChatsPerUser = MAX_PRIVATE_CHATS_PER_USER;
    size_t MaxMessageLength = MAX_MESSAGE_LENGTH;
    size_t MaxMessagesPerChat = MAX_MESSAGES_PER_CHAT;
};

enum class BanPeriod {
    OneDay,
    TenDays,
    Month,
    Year,
    Forever
};

class ChatService {
public:
    ChatService();
    explicit ChatService(ServiceLimits limits);
    explicit ChatService(storage::IManager& storageManager);
    ChatService(storage::IManager& storageManager, ServiceLimits limits);

    bool Initialize();

    bool Register(std::string&& name, std::string&& login, std::string&& password);
    bool Authenticate(const std::string& login, const std::string& password);
    std::string GetUserNameByLogin(const std::string& login) const;

    std::vector<std::string> GetMyChats(const std::string& currentLogin) const;
    bool CreatePrivateChat(const std::string& currentLogin, std::string&& recipientLogin, std::string&& chatName);
    std::string GetPrivateChatName(const std::string& firstLogin, const std::string& secondLogin) const;

    std::vector<Message> GetMessages(const std::string& currentLogin, const std::string& chatName) const;
    bool SendMessage(const std::string& currentLogin, const std::string& chatName, std::string&& text);

    std::vector<std::string> GetAllUserLogins() const;
    std::vector<UserInfo> GetAllUsersInfo() const;
    bool GetUserInfo(const std::string& login, UserInfo& info) const;
    std::vector<std::string> GetAllChatNames() const;
    std::vector<ChatInfo> GetAllChatsInfo() const;
    std::vector<ChatInfo> GetPrivateChatsInfoForAdmin() const;
    std::vector<Message> GetMessagesForAdmin(const std::string& chatName) const;

    bool SendAdminMessageToGeneral(std::string&& text);
    bool CreateAdminPrivateChat(std::string&& recipientLogin, std::string&& chatName);
    bool SendAdminMessageToChat(const std::string& chatName, std::string&& text);

    bool BanUser(const std::string& login, BanPeriod period);
    bool UnbanUser(const std::string& login);
    bool IsUserBanned(const std::string& login) const;

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

    bool PersistAdminMessage(const std::string& chatName, const Message& message);
    bool PersistUserBan(
        const std::string& login,
        std::int64_t bannedUntilEpoch,
        bool bannedForever);
    bool PersistDeletePrivateChatsWithUser(const std::string& login);

private:
    std::unordered_map<std::string, std::unique_ptr<User>> m_users;
    std::unordered_map<std::string, std::unique_ptr<BaseChat>> m_chats;
    storage::IManager* m_storageManager = nullptr;
    ServiceLimits m_limits;
};

} // namespace console_chat::core
