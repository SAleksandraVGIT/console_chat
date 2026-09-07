#include "client_worker.h"

#include "console_chat/core/chat_service.h"

#include <QThread>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace console_chat::qt {
namespace {

QString Text(const std::string& text)
{
    return QString::fromUtf8(text.data(), text.size());
}

std::string Bytes(const QString& text)
{
    return text.toUtf8().toStdString();
}

QString BanDescription(const std::string& status, const std::string& until, const std::string& now) {
    if (status == "FOREVER") {
        return "Banned forever";
    }

    if (status != "TEMP") {
        return "Active";
    }

    const auto seconds = std::max<qint64>(0, Text(until).toLongLong() - Text(now).toLongLong());
    const auto minutes = (seconds + 59) / 60;

    return QString("Banned: %1d %2h %3m remaining")
        .arg(minutes / 1440).arg(minutes / 60 % 24).arg(minutes % 60);
}

QString DisplayLogin(const std::string& login) {
    return login == core::ADMIN_SYSTEM_LOGIN ? "ADMIN" : Text(login);
}

void CheckInterruption() {
    if (QThread::currentThread()->isInterruptionRequested()) {
        throw std::runtime_error("Connection closed.");
    }
}

} // namespace

void ClientWorker::Execute(const std::function<void()>& operation) {
    try {
        CheckInterruption();
        operation();
    } catch (const std::exception& exception) {
        Reset();
        emit error(QString::fromUtf8(exception.what()));
    }
    emit finished();
}

void ClientWorker::Reset() {
    m_client.reset();
    m_state = {};
    emit snapshotReady(m_state);
}

void ClientWorker::Login(const LoginRequest& request) {
    Reset();
    m_client = std::make_unique<client::ChatClient>(Bytes(request.host), request.port,
                                                   std::chrono::seconds{5});
    if (request.registration) {
        if (request.admin) {
            emit error("Administrator accounts are configured on the server.");
            m_client.reset();
            return;
        }

        if (!m_client->Register(Bytes(request.name), Bytes(request.login), Bytes(request.password))) {
            emit error("Registration failed. Login is unavailable or the user limit was reached.");
            m_client.reset();
            return;
        }
    }

    if (request.admin) {
        if (!m_client->AdminLogin(Bytes(request.login), Bytes(request.password))) {
            emit error("Invalid administrator credentials or administrator login is disabled.");
            m_client.reset();
            return;
        }
    } else {
        const auto result = m_client->AuthenticateDetailed(Bytes(request.login), Bytes(request.password));
        if (!result.Success) {
            emit error(result.Error == "banned"
                ? BanDescription(result.BanStatus, result.BannedUntilEpoch, result.ServerNowEpoch)
                : "Invalid login or password.");
            m_client.reset();
            return;
        }
    }

    m_state.authenticated = true;
    m_state.admin = request.admin;
    m_state.login = Text(m_client->GetCurrentUserLogin());
    m_state.name = Text(m_client->GetCurrentUserName());
    m_state.selectedChat = core::GENERAL_CHAT_NAME;
    Refresh(false);
}

void ClientWorker::Refresh(const bool background) {
    if (!m_client || !m_state.authenticated) {
        return;
    }

    CheckInterruption();
    m_state.chats.clear();
    m_state.users.clear();

    if (m_state.admin) {
        for (const auto& chat : m_client->AdminGetChats(background)) {
            const bool general = chat.Name == core::GENERAL_CHAT_NAME;
            m_state.chats.push_back({Text(chat.Name), general ? "General chat"
                : DisplayLogin(chat.FirstUserLogin) + " / " + DisplayLogin(chat.SecondUserLogin),
                general || chat.FirstUserLogin == core::ADMIN_SYSTEM_LOGIN ||
                           chat.SecondUserLogin == core::ADMIN_SYSTEM_LOGIN});
        }

        CheckInterruption();

        for (const auto& user : m_client->AdminGetUsers(background)) {
            m_state.users.push_back({Text(user.Login), Text(user.Name),
                BanDescription(user.BanStatus, user.BannedUntilEpoch, user.ServerNowEpoch)});
        }
    } else {
        for (const auto& chat : m_client->GetMyChats(background)) {
            m_state.chats.push_back({Text(chat), chat == core::GENERAL_CHAT_NAME ? "General chat" : "Private chat", true});
        }
        CheckInterruption();
        for (const auto& login : m_client->GetAllUserLogins(background)) {
            m_state.users.push_back({Text(login), {}, {}});
        }
    }

    const auto selected = std::find_if(m_state.chats.begin(), m_state.chats.end(),
        [this](const ChatItem& chat) {
            return chat.name == m_state.selectedChat;
        });

    if (selected == m_state.chats.end()) {
        m_state.selectedChat = m_state.chats.isEmpty() ? QString{} : m_state.chats.front().name;
    }

    CheckInterruption();
    m_state.messages.clear();
    if (!m_state.selectedChat.isEmpty()) {
        const auto messages = m_state.admin
            ? m_client->AdminGetMessages(Bytes(m_state.selectedChat), background)
            : m_client->GetMessages(Bytes(m_state.selectedChat), background);
        for (const auto& message : messages) {
            m_state.messages.push_back({Text(message.Name), Text(message.Text)});
        }
    }
    emit snapshotReady(m_state);
}

void ClientWorker::OpenChat(const QString& name) {
    if (!m_client) {
        return;
    }
    m_state.selectedChat = name;
    Refresh(false);
}

void ClientWorker::SendMessage(const QString& text) {
    if (!m_client) {
        return;
    }

    const auto selected = std::find_if(m_state.chats.begin(), m_state.chats.end(),
        [this](const ChatItem& item) {
            return item.name == m_state.selectedChat;
        });

    if (selected == m_state.chats.end() || !selected->writable) {
        emit error("This chat is read-only.");
        return;
    }

    const auto chat = m_state.selectedChat;
    const bool success = m_state.admin
        ? (chat == core::GENERAL_CHAT_NAME ? m_client->AdminSendGeneral(Bytes(text))
            : m_client->AdminSendMessageToChat(Bytes(chat), Bytes(text)))
        : m_client->SendMessage(Bytes(chat), Bytes(text));

    if (!success) {
        emit error("Message was not sent. Check the message length and server limits.");
    }
    else {
        emit messageSent(chat, text);
    }
    Refresh(true);
}

void ClientWorker::CreateChat(const QString& recipient, const QString& name) {
    if (!m_client) {
        return;
    }

    const auto result = m_state.admin
        ? m_client->AdminCreatePrivateChatDetailed(Bytes(recipient), Bytes(name))
        : m_client->CreatePrivateChatDetailed(Bytes(recipient), Bytes(name));

    if (result.Success) {
        m_state.selectedChat = name;
    }
    else if (!result.ExistingChatName.empty()) {
        emit chatAlreadyExists(Text(result.ExistingChatName));
    } else if (result.NameInUse) {
        emit chatNameInUse(name);
    } else {
        emit error("Could not create chat. Check the recipient, chat name and server limits.");
    }
    Refresh(true);
}

void ClientWorker::Moderate(const UserAction action, const QString& login, const QString& period) {
    if (!m_client || !m_state.admin) {
        return;
    }

    bool success = false;
    switch (action) {
        case UserAction::Disconnect:
            success = m_client->AdminKickUser(Bytes(login));
            break;
        case UserAction::Ban:
            success = m_client->AdminBanUser(Bytes(login), Bytes(period));
            break;
        case UserAction::Unban:
            success = m_client->AdminUnbanUser(Bytes(login));
            break;
    }

    if (!success) {
        emit error("Operation failed. The user may be missing or already disconnected.");
    }
    else {
        emit notice("User updated: " + login);
    }
    Refresh(true);
}

void ClientWorker::DeleteAccount(const QString& confirmation) {
    if (!m_client || m_state.admin) {
        return;
    }

    if (!m_client->DeleteAccount(Bytes(confirmation))) {
        emit error("Account deletion failed. Confirmation must match your login.");
        return;
    }

    Reset();
    emit notice("Account deleted.");
}

void ClientWorker::DeleteForeverBanned() {
    if (!m_client || !m_state.admin) {
        return;
    }

    const int count = m_client->AdminDeleteForeverBannedUsers();
    if (count < 0) {
        emit error("Account cleanup failed.");
    }
    else {
        emit notice(QString("Deleted permanently banned accounts: %1").arg(count));
    }
    Refresh(true);
}

void ClientWorker::Logout() {
    if (m_client) {
        m_client->Logout();
    }
    Reset();
}

void ClientWorker::Activity() {
    if (m_client && m_state.authenticated) {
        m_client->NotifyActivity();
    }
}

} // namespace console_chat::qt
