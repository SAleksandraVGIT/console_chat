#include "console_chat/chat_service.h"
#include "console_chat/private_chat.h"

#include <algorithm>


namespace console_chat {

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

    if (!std::get<std::unique_ptr<console_chat::User>>(*it)->CheckPassword(password)) {
        return false;
    }

    m_currentUserLogin = login;
    return true;
}

std::vector<std::string> ChatService::GetMyChats() const {
    std::vector<std::string> result;

    if (m_currentUserLogin.empty()) {
        return result;
    }

    result.reserve(m_chats.size());

    for (const auto& [chatName, chat] : m_chats){
        if (chat->IsParticipant(m_currentUserLogin)) {
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
    std::string&& recipientLogin,
    std::string&& chatName)
{
    if (m_currentUserLogin.empty()) {
        return false;
    }

    if (m_chats.size() >= MAX_CHATS_ON_SERVER) {
        return false;
    }

    if (recipientLogin == m_currentUserLogin) {
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

        const bool involvesMe = privateChat->HasUser(m_currentUserLogin);
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
        std::make_unique<PrivateChat>(m_currentUserLogin, recipientLogin)
    );

    return inserted;
}

std::vector<Message> ChatService::GetMessages(const std::string& chatName) const {
    std::vector<Message> result;

    if (m_currentUserLogin.empty()) {
        return result;
    }

    BaseChat* chat = FindChat(chatName);
    if (!chat) {
        return result;
    }

    if (!chat->IsParticipant(m_currentUserLogin)) {
        return result;
    }

    return chat->GetMessages();
}

bool ChatService::SendMessage(const std::string& chatName, std::string&& text) {
    if (m_currentUserLogin.empty()) {
        return false;
    }

    if (text.empty() || text.size() > MAX_MESSAGE_LENGTH) {
        return false;
    }

    const auto it = m_chats.find(chatName);
    if (it == m_chats.end()) {
        return false;
    }

    auto* chat = std::get<std::unique_ptr<console_chat::BaseChat>>(*it).get();

    if (!chat->IsParticipant(m_currentUserLogin)) {
        return false;
    }

    const auto userIt = m_users.find(m_currentUserLogin);
    if (userIt == m_users.end()) {
        return false;
    }

    return chat->AddMessage(Message{
            std::get<std::unique_ptr<console_chat::User>>(*userIt)->GetName(),
            std::move(text)
        });
}

User* ChatService::FindUser(const std::string& login) const {
    const auto it = m_users.find(login);
    return it != m_users.end() ? std::get<std::unique_ptr<console_chat::User>>(*it).get() : nullptr;
}

BaseChat* ChatService::FindChat(const std::string& name) const {
    const auto it = m_chats.find(name);
    return it != m_chats.end() ? std::get<std::unique_ptr<console_chat::BaseChat>>(*it).get() : nullptr;
}

} // namespace console_chat
