#include "console_chat/core/chat_service.h"
#include "console_chat/core/password_protector.h"
#include "console_chat/core/private_chat.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>


namespace console_chat::core {

constexpr size_t MAX_CHATS_ON_SERVER = 991;
constexpr size_t MAX_PRIVATE_CHATS_PER_USER = 45;
constexpr size_t MAX_MESSAGE_LENGTH = 256;

namespace {

bool SetOwnerOnlyPermissions(const std::string& filePath) {
#ifdef _WIN32
    (void)filePath;
    return true;
#else
    std::error_code ec;
    std::filesystem::permissions(
        filePath,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace,
        ec);
    return !ec;
#endif
}

} // namespace

ChatService::ChatService() {
    m_chats.try_emplace(GENERAL_CHAT_NAME, std::make_unique<BaseChat>());
}

bool ChatService::Register(
    std::string&& name,
    std::string&& login,
    std::string&& password)
{
    std::string passwordHash = PasswordProtector::Hash(password);
    const auto [_, inserted] =
        m_users.try_emplace(login, std::make_unique<User>(std::move(name), std::move(passwordHash)));

    return inserted;
}

bool ChatService::Authenticate(const std::string& login, const std::string& password) {
    const auto it = m_users.find(login);
    if (it == m_users.end()) {
        return false;
    }

    if (!std::get<std::unique_ptr<User>>(*it)->CheckPassword(password)) {
        return false;
    }

    return true;
}

std::string ChatService::GetUserNameByLogin(const std::string& login) const {
    const auto it = m_users.find(login);
    return it != m_users.end() ? std::get<std::unique_ptr<User>>(*it)->GetName() : std::string{};
}

std::vector<std::string> ChatService::GetMyChats(const std::string& currentLogin) const {
    std::vector<std::string> result;

    if (currentLogin.empty() || !m_users.contains(currentLogin)) {
        return result;
    }

    result.reserve(m_chats.size());

    for (const auto& [chatName, chat] : m_chats){
        if (chat->IsParticipant(currentLogin)) {
            result.emplace_back(chatName);
        }
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> ChatService::GetAllUserLogins() const {
    std::vector<std::string> result;
    result.reserve(m_users.size());

    for (const auto& [login, _] : m_users) {
        result.emplace_back(login);
    }

    std::sort(result.begin(), result.end());
    return result;
}

bool ChatService::CreatePrivateChat(
    const std::string& currentLogin,
    std::string&& recipientLogin,
    std::string&& chatName)
{
    if (currentLogin.empty() || !m_users.contains(currentLogin)) {
        return false;
    }

    if (m_chats.size() >= MAX_CHATS_ON_SERVER) {
        return false;
    }

    if (recipientLogin == currentLogin) {
        return false;
    }

    if (m_chats.contains(chatName)) {
        return false;
    }

    if (!m_users.contains(recipientLogin)) {
        return false;
    }

    size_t privateCountMe = 0;
    size_t privateCountRecipient = 0;

    for (const auto& [_, chat] : m_chats) {
        auto* privateChat = dynamic_cast<PrivateChat*>(chat.get());
        if (!privateChat ) {
            continue;
        }

        const bool involvesMe = privateChat->HasUser(currentLogin);
        const bool involvesRecipient = privateChat->HasUser(recipientLogin);

        if (involvesMe && involvesRecipient) {
            return false;
        }

        if (involvesMe) {
            ++privateCountMe;
        }

        if (involvesRecipient) {
            ++privateCountRecipient;
        }
    }

    if (privateCountMe >= MAX_PRIVATE_CHATS_PER_USER ||
        privateCountRecipient >= MAX_PRIVATE_CHATS_PER_USER)
    {
        return false;
    }

    const auto [_, inserted] = m_chats.try_emplace(
        std::move(chatName),
        std::make_unique<PrivateChat>(currentLogin, recipientLogin)
    );

    return inserted;
}

std::vector<Message> ChatService::GetMessages(const std::string& currentLogin, const std::string& chatName) const {
    std::vector<Message> result;

    if (currentLogin.empty() || !m_users.contains(currentLogin)) {
        return result;
    }

    BaseChat* chat = FindChat(chatName);
    if (!chat) {
        return result;
    }

    if (!chat->IsParticipant(currentLogin)) {
        return result;
    }

    return chat->GetMessages();
}

bool ChatService::SendMessage(const std::string& currentLogin, const std::string& chatName, std::string&& text) {
    if (currentLogin.empty() || !m_users.contains(currentLogin)) {
        return false;
    }

    if (text.empty() || text.size() > MAX_MESSAGE_LENGTH) {
        return false;
    }

    const auto it = m_chats.find(chatName);
    if (it == m_chats.end()) {
        return false;
    }

    auto* chat = std::get<std::unique_ptr<BaseChat>>(*it).get();

    if (!chat->IsParticipant(currentLogin)) {
        return false;
    }

    const auto userIt = m_users.find(currentLogin);
    if (userIt == m_users.end()) {
        return false;
    }

    return chat->AddMessage(Message{
            std::get<std::unique_ptr<User>>(*userIt)->GetName(),
            std::move(text)
        });
}

User* ChatService::FindUser(const std::string& login) const {
    const auto it = m_users.find(login);
    return it != m_users.end() ? std::get<std::unique_ptr<User>>(*it).get() : nullptr;
}

BaseChat* ChatService::FindChat(const std::string& name) const {
    const auto it = m_chats.find(name);
    return it != m_chats.end() ? std::get<std::unique_ptr<BaseChat>>(*it).get() : nullptr;
}

bool ChatService::SaveState(const std::string& usersFilePath, const std::string& chatsFilePath) const {
    {
        std::ofstream usersOut(usersFilePath, std::ios::trunc);
        if (!usersOut) {
            return false;
        }

        usersOut << m_users.size() << '\n';
        for (const auto& [login, user] : m_users) {
            usersOut << std::quoted(login) << ' '
                     << std::quoted(user->GetName()) << ' '
                     << std::quoted(user->GetPasswordHash()) << '\n';
        }
    }
    if (!SetOwnerOnlyPermissions(usersFilePath)) {
        return false;
    }

    {
        std::ofstream chatsOut(chatsFilePath, std::ios::trunc);
        if (!chatsOut) {
            return false;
        }

        chatsOut << m_chats.size() << '\n';
        for (const auto& [chatName, chat] : m_chats) {
            const auto* privateChat = dynamic_cast<const PrivateChat*>(chat.get());
            const int isPrivate = privateChat ? 1 : 0;

            chatsOut << std::quoted(chatName) << ' ' << isPrivate << '\n';
            if (isPrivate) {
                const auto& users = privateChat->GetUsers();
                chatsOut << std::quoted(users[0]) << ' ' << std::quoted(users[1]) << '\n';
            }

            const auto& messages = chat->GetMessages();
            chatsOut << messages.size() << '\n';
            for (const auto& msg : messages) {
                chatsOut << std::quoted(msg.Name) << ' ' << std::quoted(msg.Text) << '\n';
            }
        }
    }
    if (!SetOwnerOnlyPermissions(chatsFilePath)) {
        return false;
    }

    return true;
}

bool ChatService::LoadState(const std::string& usersFilePath, const std::string& chatsFilePath) {
    std::ifstream usersIn(usersFilePath);
    std::ifstream chatsIn(chatsFilePath);
    if (!usersIn || !chatsIn) {
        return false;
    }

    std::unordered_map<std::string, std::unique_ptr<User>> loadedUsers;
    std::unordered_map<std::string, std::unique_ptr<BaseChat>> loadedChats;

    size_t usersCount = 0;
    if (!(usersIn >> usersCount)) {
        return false;
    }

    for (size_t i = 0; i < usersCount; ++i) {
        std::string login;
        std::string name;
        std::string passwordHash;
        if (!(usersIn >> std::quoted(login) >> std::quoted(name) >> std::quoted(passwordHash))) {
            return false;
        }

        if (!PasswordProtector::IsHash(passwordHash)) {
            return false;
        }

        loadedUsers.try_emplace(
            login,
            std::make_unique<User>(std::move(name), std::move(passwordHash)));
    }

    size_t chatsCount = 0;
    if (!(chatsIn >> chatsCount)) {
        return false;
    }

    for (size_t i = 0; i < chatsCount; ++i) {
        std::string chatName;
        int isPrivate = 0;
        if (!(chatsIn >> std::quoted(chatName) >> isPrivate)) {
            return false;
        }

        std::unique_ptr<BaseChat> chat;
        if (isPrivate == 1) {
            std::string user1;
            std::string user2;
            if (!(chatsIn >> std::quoted(user1) >> std::quoted(user2))) {
                return false;
            }
            chat = std::make_unique<PrivateChat>(std::move(user1), std::move(user2));
        } else {
            chat = std::make_unique<BaseChat>();
        }

        size_t messagesCount = 0;
        if (!(chatsIn >> messagesCount)) {
            return false;
        }
        for (size_t m = 0; m < messagesCount; ++m) {
            std::string senderName;
            std::string text;
            if (!(chatsIn >> std::quoted(senderName) >> std::quoted(text))) {
                return false;
            }
            if (!chat->AddMessage(Message{std::move(senderName), std::move(text)})) {
                return false;
            }
        }

        loadedChats.try_emplace(std::move(chatName), std::move(chat));
    }

    if (!loadedChats.contains(GENERAL_CHAT_NAME)) {
        loadedChats.try_emplace(GENERAL_CHAT_NAME, std::make_unique<BaseChat>());
    }

    m_users = std::move(loadedUsers);
    m_chats = std::move(loadedChats);
    return true;
}

} // namespace console_chat::core
