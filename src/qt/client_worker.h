#pragma once

#include "session_types.h"
#include "console_chat/client/chat_client.h"

#include <QObject>
#include <functional>
#include <memory>

namespace console_chat::qt {

class ClientWorker final : public QObject {
    Q_OBJECT
public:
    void Execute(const std::function<void()>& operation);
    void Login(const LoginRequest& request);
    void Refresh(bool background = true);
    void OpenChat(const QString& name);
    void SendMessage(const QString& text);
    void CreateChat(const QString& recipient, const QString& name);
    void Moderate(UserAction action, const QString& login, const QString& period);

    void DeleteAccount(const QString& confirmation);
    void DeleteForeverBanned();

    void Logout();
    void Activity();

signals:
    void snapshotReady(const SessionSnapshot& snapshot);
    void error(const QString& message);
    void notice(const QString& message);
    void chatAlreadyExists(const QString& name);
    void chatNameInUse(const QString& name);
    void messageSent(const QString& chat, const QString& text);
    void finished();

private:
    void Reset();

private:
    std::unique_ptr<client::ChatClient> m_client;
    SessionSnapshot m_state;
};

} // namespace console_chat::qt
