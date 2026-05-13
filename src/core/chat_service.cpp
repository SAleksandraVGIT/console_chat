#include "console_chat/core/chat_service.h"
#include "console_chat/core/private_chat.h"

#include <algorithm>


namespace console_chat::core {

constexpr size_t MAX_CHATS_ON_SERVER = 991;
constexpr size_t MAX_PRIVATE_CHATS_PER_USER = 45;
constexpr size_t MAX_MESSAGE_LENGTH = 256;

ChatService::ChatService() {
    m_chats.try_emplace(GENERAL_CHAT_NAME, std::make_unique<BaseChat>());
}

bool ChatService::Register(
    std::string&& name,
    std::string&& login,
    std::string&& password)
{
    const auto [_, inserted] =
        m_users.try_emplace(login, std::make_unique<User>(std::move(name), std::move(password)));

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

} // namespace console_chat::core
