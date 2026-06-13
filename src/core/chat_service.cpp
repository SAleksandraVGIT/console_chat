#include "console_chat/core/chat_service.h"
#include "console_chat/core/password_protector.h"
#include "console_chat/core/private_chat.h"

#include <algorithm>

namespace console_chat::core {

constexpr size_t MAX_CHATS_ON_SERVER = 991;

ChatService::ChatService() {
    m_chats.try_emplace(GENERAL_CHAT_NAME, std::make_unique<BaseChat>());
}

ChatService::ChatService(storage::IManager& storageManager)
    : ChatService()
{
    m_storageManager = &storageManager;
}

bool ChatService::Initialize() {
    if (!m_storageManager) {
        return true;
    }

    if (!m_storageManager->Initialize()) {
        return false;
    }

    ServiceState state;
    if (!m_storageManager->Load(state)) {
        return false;
    }

    const bool hasGeneralChat = std::any_of(
        state.Chats.begin(),
        state.Chats.end(),
        [](const ChatState& chat) { return chat.Name == GENERAL_CHAT_NAME; });

    if (!ImportState(std::move(state))) {
        return false;
    }

    if (!hasGeneralChat) {
        ChatState generalChat;
        generalChat.Name = GENERAL_CHAT_NAME;
        return PersistChat(generalChat);
    }

    return true;
}

bool ChatService::Register(
    std::string&& name,
    std::string&& login,
    std::string&& password)
{
    if (m_users.contains(login)) {
        return false;
    }

    std::string passwordHash = PasswordProtector::Hash(password);
    const UserState userState{login, name, passwordHash};
    if (!PersistUser(userState)) {
        return false;
    }

    const auto [_, inserted] =
        m_users.try_emplace(std::move(login), std::make_unique<User>(std::move(name), std::move(passwordHash)));

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

    ChatState chatState;
    chatState.Name = chatName;
    chatState.IsPrivate = true;
    chatState.Participants = {currentLogin, recipientLogin};
    if (!PersistChat(chatState)) {
        return false;
    }

    const auto [_, inserted] = m_chats.try_emplace(
        std::move(chatName),
        std::make_unique<PrivateChat>(currentLogin, std::move(recipientLogin)));

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

    if (chat->GetMessages().size() >= MAX_MESSAGES_PER_CHAT) {
        return false;
    }

    const auto userIt = m_users.find(currentLogin);
    if (userIt == m_users.end()) {
        return false;
    }

    Message message{
        std::get<std::unique_ptr<User>>(*userIt)->GetName(),
        std::move(text)};
    if (!PersistMessage(chatName, currentLogin, message)) {
        return false;
    }

    return chat->AddMessage(std::move(message));
}

User* ChatService::FindUser(const std::string& login) const {
    const auto it = m_users.find(login);
    return it != m_users.end() ? std::get<std::unique_ptr<User>>(*it).get() : nullptr;
}

BaseChat* ChatService::FindChat(const std::string& name) const {
    const auto it = m_chats.find(name);
    return it != m_chats.end() ? std::get<std::unique_ptr<BaseChat>>(*it).get() : nullptr;
}

bool ChatService::ImportState(ServiceState state) {
    std::unordered_map<std::string, std::unique_ptr<User>> loadedUsers;
    std::unordered_map<std::string, std::unique_ptr<BaseChat>> loadedChats;

    if (state.Chats.size() > MAX_CHATS_ON_SERVER) {
        return false;
    }

    loadedUsers.reserve(state.Users.size());
    for (auto& user : state.Users) {
        if (!PasswordProtector::IsHash(user.PasswordHash)) {
            return false;
        }

        const auto [_, inserted] = loadedUsers.try_emplace(
            std::move(user.Login),
            std::make_unique<User>(std::move(user.Name), std::move(user.PasswordHash)));
        if (!inserted) {
            return false;
        }
    }

    loadedChats.reserve(state.Chats.size() + 1);
    for (auto& chatState : state.Chats) {
        std::unique_ptr<BaseChat> chat;
        if (chatState.IsPrivate) {
            if (!loadedUsers.contains(chatState.Participants[0]) ||
                !loadedUsers.contains(chatState.Participants[1]))
            {
                return false;
            }
            chat = std::make_unique<PrivateChat>(
                std::move(chatState.Participants[0]),
                std::move(chatState.Participants[1]));
        } else {
            chat = std::make_unique<BaseChat>();
        }

        for (auto& message : chatState.Messages) {
            if (!chat->AddMessage(std::move(message))) {
                return false;
            }
        }

        const auto [_, inserted] = loadedChats.try_emplace(std::move(chatState.Name), std::move(chat));
        if (!inserted) {
            return false;
        }
    }

    if (!loadedChats.contains(GENERAL_CHAT_NAME)) {
        loadedChats.try_emplace(GENERAL_CHAT_NAME, std::make_unique<BaseChat>());
    }

    m_users = std::move(loadedUsers);
    m_chats = std::move(loadedChats);
    return true;
}

bool ChatService::PersistUser(const UserState& user) {
    return !m_storageManager || m_storageManager->AddUser(user);
}

bool ChatService::PersistChat(const ChatState& chat) {
    return !m_storageManager || m_storageManager->AddChat(chat);
}

bool ChatService::PersistMessage(
    const std::string& chatName,
    const std::string& senderLogin,
    const Message& message)
{
    return !m_storageManager ||
        m_storageManager->AddMessage(chatName, senderLogin, message);
}

} // namespace console_chat::core
