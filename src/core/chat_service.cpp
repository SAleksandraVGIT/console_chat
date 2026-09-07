#include "console_chat/core/chat_service.h"
#include "console_chat/core/password_protector.h"
#include "console_chat/core/private_chat.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <string_view>
#include <utility>

namespace console_chat::core {

std::int64_t CurrentEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::int64_t BanEndEpoch(BanPeriod period) {
    const auto now = CurrentEpochSeconds();
    switch (period) {
        case BanPeriod::OneDay:
            return now + 24 * 60 * 60;
        case BanPeriod::TenDays:
            return now + 10 * 24 * 60 * 60;
        case BanPeriod::Month:
            return now + 30 * 24 * 60 * 60;
        case BanPeriod::Year:
            return now + 365 * 24 * 60 * 60;
        case BanPeriod::Forever:
            return 0;
    }

    return 0;
}

bool IsPrivateChatBetween(
    const PrivateChat& chat,
    const std::string& firstLogin,
    const std::string& secondLogin)
{
    const auto& users = chat.GetUsers();
    return (users[0] == firstLogin && users[1] == secondLogin) ||
        (users[0] == secondLogin && users[1] == firstLogin);
}

bool IsAdminDisplayLogin(const std::string& login) {
    const std::string_view adminLogin = ADMIN_MESSAGE_NAME;
    return login.size() == adminLogin.size() &&
        std::equal(
            login.begin(),
            login.end(),
            adminLogin.begin(),
            [](const char left, const char right) {
                return std::tolower(static_cast<unsigned char>(left)) ==
                    std::tolower(static_cast<unsigned char>(right));
            });
}

bool IsReservedLogin(const std::string& login) {
    return login == ADMIN_SYSTEM_LOGIN || IsAdminDisplayLogin(login);
}

ChatService::ChatService()
    : ChatService(ServiceLimits{}) {}

ChatService::ChatService(ServiceLimits limits)
    : m_limits(std::move(limits))
{
    m_chats.try_emplace(
        GENERAL_CHAT_NAME,
        std::make_unique<BaseChat>(m_limits.MaxMessagesPerChat));
}

ChatService::ChatService(storage::IManager& storageManager)
    : ChatService(storageManager, ServiceLimits{}) {}

ChatService::ChatService(storage::IManager& storageManager, ServiceLimits limits)
    : ChatService(std::move(limits))
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
        [](const ChatState& chat) {
            return chat.Name == GENERAL_CHAT_NAME;
        });

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
    if (IsReservedLogin(login) || m_users.contains(login))
    {
        return false;
    }

    if (m_limits.MaxUsers != 0 && m_users.size() >= m_limits.MaxUsers) {
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

    const auto& user = std::get<std::unique_ptr<User>>(*it);
    if (user->IsBannedAt(CurrentEpochSeconds())) {
        return false;
    }

    if (!user->CheckPassword(password)) {
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

    if (currentLogin.empty() || !m_users.contains(currentLogin) || IsUserBanned(currentLogin)) {
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

std::string ChatService::GetPrivateChatName(
    const std::string& firstLogin,
    const std::string& secondLogin) const
{
    if (firstLogin.empty() || secondLogin.empty()) {
        return {};
    }

    for (const auto& [name, chat] : m_chats) {
        const auto* privateChat = dynamic_cast<const PrivateChat*>(chat.get());
        if (privateChat && IsPrivateChatBetween(*privateChat, firstLogin, secondLogin)) {
            return name;
        }
    }

    return {};
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

std::vector<UserInfo> ChatService::GetAllUsersInfo() const {
    std::vector<UserInfo> result;
    result.reserve(m_users.size());

    const auto now = CurrentEpochSeconds();
    for (const auto& [login, user] : m_users) {
        result.push_back(UserInfo{
            login,
            user->GetName(),
            user->GetBannedUntilEpoch(),
            user->IsBannedForever(),
            user->IsBannedAt(now)});
    }

    std::sort(result.begin(), result.end(),
        [](const UserInfo& left, const UserInfo& right) {
            return left.Login < right.Login;
        });
    return result;
}

bool ChatService::GetUserInfo(const std::string& login, UserInfo& info) const {
    const User* user = FindUser(login);
    if (!user) {
        return false;
    }

    const auto now = CurrentEpochSeconds();
    info = UserInfo{
        login,
        user->GetName(),
        user->GetBannedUntilEpoch(),
        user->IsBannedForever(),
        user->IsBannedAt(now)};
    return true;
}

bool ChatService::ChatNameExists(const std::string& name) const
{
    return m_chats.contains(name);
}

std::vector<std::string> ChatService::GetAllChatNames() const {
    std::vector<std::string> result;
    result.reserve(m_chats.size());

    for (const auto& [name, _] : m_chats) {
        result.emplace_back(name);
    }

    std::sort(result.begin(), result.end());
    return result;
}

std::vector<ChatInfo> ChatService::GetAllChatsInfo() const {
    std::vector<ChatInfo> result;
    result.reserve(m_chats.size());

    for (const auto& [name, chat] : m_chats) {
        ChatInfo info;
        info.Name = name;
        info.IsPrivate = chat->IsPrivate();
        if (const auto* privateChat = dynamic_cast<const PrivateChat*>(chat.get())) {
            info.Participants = privateChat->GetUsers();
        }
        result.push_back(std::move(info));
    }

    std::sort(result.begin(), result.end(),
        [](const ChatInfo& left, const ChatInfo& right) {
            return left.Name < right.Name;
        });
    return result;
}

std::vector<ChatInfo> ChatService::GetPrivateChatsInfoForAdmin() const {
    std::vector<ChatInfo> result;
    result.reserve(m_chats.size());

    for (const auto& [name, chat] : m_chats) {
        const auto* privateChat = dynamic_cast<const PrivateChat*>(chat.get());
        if (!privateChat) {
            continue;
        }

        ChatInfo info;
        info.Name = name;
        info.IsPrivate = true;
        info.Participants = privateChat->GetUsers();
        result.push_back(std::move(info));
    }

    std::sort(result.begin(), result.end(),
        [](const ChatInfo& left, const ChatInfo& right) {
            return left.Name < right.Name;
        });
    return result;
}

std::vector<Message> ChatService::GetMessagesForAdmin(const std::string& chatName) const {
    BaseChat* chat = FindChat(chatName);
    return chat ? chat->GetMessages() : std::vector<Message>{};
}

bool ChatService::SendAdminMessageToGeneral(std::string&& text) {
    if (text.empty() || text.size() > m_limits.MaxMessageLength) {
        return false;
    }

    BaseChat* chat = FindChat(GENERAL_CHAT_NAME);
    if (!chat || chat->GetMessages().size() >= m_limits.MaxMessagesPerChat) {
        return false;
    }

    Message message{ADMIN_MESSAGE_NAME, std::move(text)};
    if (!PersistAdminMessage(GENERAL_CHAT_NAME, message)) {
        return false;
    }

    return chat->AddMessage(std::move(message));
}

bool ChatService::CreateAdminPrivateChat(
    std::string&& recipientLogin,
    std::string&& chatName)
{
    if (m_chats.size() >= m_limits.MaxChats) {
        return false;
    }

    if (chatName.empty() || m_chats.contains(chatName)) {
        return false;
    }

    if (!m_users.contains(recipientLogin) || IsUserBanned(recipientLogin)) {
        return false;
    }

    for (const auto& [_, chat] : m_chats) {
        const auto* privateChat = dynamic_cast<const PrivateChat*>(chat.get());
        if (privateChat &&
            privateChat->HasUser(ADMIN_SYSTEM_LOGIN) &&
            privateChat->HasUser(recipientLogin))
        {
            return false;
        }
    }

    ChatState chatState;
    chatState.Name = chatName;
    chatState.IsPrivate = true;
    chatState.Participants = {ADMIN_SYSTEM_LOGIN, recipientLogin};
    if (!PersistChat(chatState)) {
        return false;
    }

    const auto [_, inserted] = m_chats.try_emplace(
        std::move(chatName),
        std::make_unique<PrivateChat>(
            ADMIN_SYSTEM_LOGIN,
            std::move(recipientLogin),
            m_limits.MaxMessagesPerChat));

    return inserted;
}

bool ChatService::SendAdminMessageToChat(
    const std::string& chatName,
    std::string&& text)
{
    if (text.empty() || text.size() > m_limits.MaxMessageLength) {
        return false;
    }

    BaseChat* chat = FindChat(chatName);
    auto* privateChat = dynamic_cast<PrivateChat*>(chat);
    if (!privateChat || !privateChat->HasUser(ADMIN_SYSTEM_LOGIN)) {
        return false;
    }

    if (chat->GetMessages().size() >= m_limits.MaxMessagesPerChat) {
        return false;
    }

    Message message{ADMIN_MESSAGE_NAME, std::move(text)};
    if (!PersistAdminMessage(chatName, message)) {
        return false;
    }

    return chat->AddMessage(std::move(message));
}

bool ChatService::BanUser(const std::string& login, const BanPeriod period) {
    User* user = FindUser(login);
    if (!user) {
        return false;
    }

    const bool bannedForever = period == BanPeriod::Forever;
    const std::int64_t bannedUntilEpoch = bannedForever ? 0 : BanEndEpoch(period);

    if (!PersistUserBan(login, bannedUntilEpoch, bannedForever)) {
        return false;
    }

    user->SetBan(bannedUntilEpoch, bannedForever);
    return true;
}

bool ChatService::UnbanUser(const std::string& login) {
    User* user = FindUser(login);
    if (!user) {
        return false;
    }

    if (!PersistUserBan(login, 0, false)) {
        return false;
    }

    user->SetBan(0, false);
    return true;
}

bool ChatService::IsUserBanned(const std::string& login) const {
    const User* user = FindUser(login);
    return user && user->IsBannedAt(CurrentEpochSeconds());
}

bool ChatService::DeleteUserAccount(const std::string& login) {
    if (login.empty() || login == ADMIN_SYSTEM_LOGIN || !m_users.contains(login)) {
        return false;
    }

    if (!PersistDeleteUser(login)) {
        return false;
    }

    m_users.erase(login);
    for (auto it = m_chats.begin(); it != m_chats.end();) {
        auto* privateChat = dynamic_cast<PrivateChat*>(it->second.get());
        if (privateChat && privateChat->HasUser(login)) {
            it = m_chats.erase(it);
            continue;
        }
        ++it;
    }

    return true;
}

bool ChatService::DeleteForeverBannedUsers(size_t& deletedCount) {
    deletedCount = 0;

    std::vector<std::string> loginsToDelete;
    loginsToDelete.reserve(m_users.size());
    for (const auto& [login, user] : m_users) {
        if (user->IsBannedForever()) {
            loginsToDelete.push_back(login);
        }
    }

    std::sort(loginsToDelete.begin(), loginsToDelete.end());
    for (const auto& login : loginsToDelete) {
        if (!DeleteUserAccount(login)) {
            return false;
        }
        ++deletedCount;
    }

    return true;
}

bool ChatService::CreatePrivateChat(
    const std::string& currentLogin,
    std::string&& recipientLogin,
    std::string&& chatName)
{
    if (currentLogin.empty() || !m_users.contains(currentLogin) || IsUserBanned(currentLogin)) {
        return false;
    }

    if (m_chats.size() >= m_limits.MaxChats) {
        return false;
    }

    if (m_chats.contains(chatName)) {
        return false;
    }

    if (!m_users.contains(recipientLogin) || IsUserBanned(recipientLogin)) {
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

        if (IsPrivateChatBetween(*privateChat, currentLogin, recipientLogin)) {
            return false;
        }

        if (involvesMe) {
            ++privateCountMe;
        }

        if (involvesRecipient) {
            ++privateCountRecipient;
        }
    }

    if (privateCountMe >= m_limits.MaxPrivateChatsPerUser ||
        privateCountRecipient >= m_limits.MaxPrivateChatsPerUser)
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
        std::make_unique<PrivateChat>(
            currentLogin,
            std::move(recipientLogin),
            m_limits.MaxMessagesPerChat));

    return inserted;
}

std::vector<Message> ChatService::GetMessages(const std::string& currentLogin, const std::string& chatName) const {
    std::vector<Message> result;

    if (currentLogin.empty() || !m_users.contains(currentLogin) || IsUserBanned(currentLogin)) {
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
    if (currentLogin.empty() || !m_users.contains(currentLogin) || IsUserBanned(currentLogin)) {
        return false;
    }

    if (text.empty() || text.size() > m_limits.MaxMessageLength) {
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

    if (chat->GetMessages().size() >= m_limits.MaxMessagesPerChat) {
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

    if (state.Chats.size() > m_limits.MaxChats) {
        return false;
    }

    loadedUsers.reserve(state.Users.size());
    for (auto& user : state.Users) {
        if (!PasswordProtector::IsHash(user.PasswordHash)) {
            return false;
        }

        const auto [_, inserted] = loadedUsers.try_emplace(
            std::move(user.Login),
            std::make_unique<User>(
                std::move(user.Name),
                std::move(user.PasswordHash),
                user.BannedUntilEpoch,
                user.BannedForever));
        if (!inserted) {
            return false;
        }
    }

    loadedChats.reserve(state.Chats.size() + 1);
    for (auto& chatState : state.Chats) {
        std::unique_ptr<BaseChat> chat;
        if (chatState.IsPrivate) {
            const auto participantExists = [&loadedUsers](const std::string& login) {
                return login == ADMIN_SYSTEM_LOGIN || loadedUsers.contains(login);
            };

            if (!participantExists(chatState.Participants[0]) ||
                !participantExists(chatState.Participants[1]))
            {
                return false;
            }
            chat = std::make_unique<PrivateChat>(
                std::move(chatState.Participants[0]),
                std::move(chatState.Participants[1]),
                m_limits.MaxMessagesPerChat);
        } else {
            chat = std::make_unique<BaseChat>(m_limits.MaxMessagesPerChat);
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
        loadedChats.try_emplace(
            GENERAL_CHAT_NAME,
            std::make_unique<BaseChat>(m_limits.MaxMessagesPerChat));
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
        m_storageManager->AddMessage(
            chatName,
            senderLogin,
            message,
            m_limits.MaxMessagesPerChat);
}

bool ChatService::PersistAdminMessage(
    const std::string& chatName,
    const Message& message)
{
    return !m_storageManager ||
        m_storageManager->AddAdminMessage(
            chatName,
            message,
            m_limits.MaxMessagesPerChat);
}

bool ChatService::PersistUserBan(
    const std::string& login,
    const std::int64_t bannedUntilEpoch,
    const bool bannedForever)
{
    return !m_storageManager ||
        m_storageManager->UpdateUserBan(login, bannedUntilEpoch, bannedForever);
}

bool ChatService::PersistDeletePrivateChatsWithUser(const std::string& login) {
    return !m_storageManager ||
        m_storageManager->DeletePrivateChatsWithUser(login);
}

bool ChatService::PersistDeleteUser(const std::string& login) {
    return !m_storageManager || m_storageManager->DeleteUser(login);
}

} // namespace console_chat::core
