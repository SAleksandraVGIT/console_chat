#include "console_chat/chat_service.h"
#include "console_chat/private_chat.h"


namespace console_chat {

const size_t MAX_CHATS_ON_SERVER = 991;
const size_t MAX_PRIVATE_CHATS_PER_USER = 45;
const size_t MAX_MESSAGE_LENGTH = 256;

ChatService::ChatService() {
    m_chats.emplace_back(std::make_unique<BaseChat>(GENERAL_CHAT_NAME));
}

bool ChatService::Register(
    std::string&& name,
    std::string&& login,
    std::string&& password)
{
    if (FindUser(login)) {
        return false;
    }

    m_users.emplace_back(
        std::make_unique<User>(std::move(name), std::move(login), std::move(password)));

    return true;
}

bool ChatService::Authenticate(const std::string& login, const std::string& password) {
    User* user = FindUser(login);
    if (!user) {
        return false;
    }

    if (!user->CheckPassword(password)) {
        return false;
    }

    m_currentUser = user;
    return true;
}

std::vector<std::string> ChatService::GetMyChats() const {
    std::vector<std::string> result;

    if (!m_currentUser) {
        return result;
    }

    for (const auto& chat : m_chats){
        if (chat->IsParticipant(m_currentUser->GetLogin())) {
            result.emplace_back(chat->GetName());
        }
    }

    return result;
}

std::vector<std::string> ChatService::GetAllUserLogins() const {
    std::vector<std::string> result;

    for (const auto& user : m_users) {
        result.push_back(user->GetLogin());
    }

    return result;
}

bool ChatService::CreatePrivateChat(
    std::string&& recipientLogin,
    std::string&& chatName)
{
    if (!m_currentUser) {
        return false;
    }

    if (m_chats.size() >= MAX_CHATS_ON_SERVER) {
        return false;
    }

    if (recipientLogin == m_currentUser->GetLogin()) {
        return false;
    }

    if (FindChat(chatName)) {
        return false;
    }

    User* recipient = FindUser(recipientLogin);
    if (!recipient) {
        return false;
    }

    std::string currentUserLogin = m_currentUser->GetLogin();

    size_t privateCountMe = 0;
    size_t privateCountRecipient = 0;
    for (const auto& chat : m_chats) {
        if (chat->IsPrivate()) {
            auto* pc = dynamic_cast<PrivateChat*>(chat.get());
            if (!pc) {
                continue;
            }

            const bool involvesMe = (pc->User1() == currentUserLogin || pc->User2() == currentUserLogin);
            const bool involvesRecipient = (pc->User1() == recipientLogin || pc->User2() == recipientLogin);

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
    }

    if (privateCountMe >= MAX_PRIVATE_CHATS_PER_USER ||
        privateCountRecipient >= MAX_PRIVATE_CHATS_PER_USER)
    {
        return false;
    }

    m_chats.emplace_back(
        std::make_unique<PrivateChat>(std::move(chatName), std::move(currentUserLogin), std::move(recipientLogin)));

    return true;
}

std::vector<Message> ChatService::GetMessages(const std::string& chatName) const {
    std::vector<Message> result;

    if (!m_currentUser) {
        return result;
    }

    BaseChat* chat = FindChat(chatName);
    if (!chat) {
        return result;
    }

    if (!chat->IsParticipant(m_currentUser->GetLogin())) {
        return result;
    }

    return chat->GetMessages();
}

bool ChatService::SendMessage(const std::string& chatName, std::string&& text) {
    if (!m_currentUser) {
        return false;
    }

     if (text.empty() || text.size() > MAX_MESSAGE_LENGTH) {
        return false;
    }

    BaseChat* chat = FindChat(chatName);
    if (!chat) {
        return false;
    }

    if (!chat->IsParticipant(m_currentUser->GetLogin())) {
        return false;
    }

    return chat->AddMessage(Message{m_currentUser->GetName(), std::move(text)});
}

User* ChatService::FindUser(const std::string& login) const {
    for (const auto& user : m_users) {
        if (user->GetLogin() == login) {
            return user.get();
        }
    }

    return nullptr;
}

BaseChat* ChatService::FindChat(const std::string& name) const {
    for (const auto& chat : m_chats) {
        if (chat->GetName() == name) {
            return chat.get();
        }
    }

    return nullptr;
}

} // namespace console_chat
