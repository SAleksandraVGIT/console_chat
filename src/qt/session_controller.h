#pragma once

#include "session_types.h"

#include <QObject>
#include <QThread>
#include <QTimer>
#include <functional>
#include <deque>

namespace console_chat::qt {

class ClientWorker;

class SessionController final : public QObject {
    Q_OBJECT
public:
    explicit SessionController(int refreshIntervalMs, QObject* parent = nullptr);

    ~SessionController() override;

    const SessionSnapshot& State() const
    {
        return m_state;
    }
    bool Busy() const
    {
        return m_busy;
    }

    void Login(const LoginRequest& request);
    void OpenChat(const QString& name);
    void SendMessage(const QString& text);
    void CreateChat(const QString& recipient, const QString& name);
    void Moderate(UserAction action, const QString& login, const QString& period = {});
    void DeleteAccount(const QString& confirmation);
    void DeleteForeverBanned();
    void Logout();
    void Refresh();
    void RecordActivity();

signals:
    void snapshotChanged(const SessionSnapshot& snapshot);
    void busyChanged(bool busy);
    void error(const QString& message);
    void notice(const QString& message);
    void chatAlreadyExists(const QString& name);
    void chatNameInUse(const QString& name);
    void messageSent(const QString& chat, const QString& text);

private:
    void Dispatch(std::function<void(ClientWorker&)> operation, bool foreground = true);

private:
    ClientWorker* m_worker;
    QThread m_thread;
    QTimer m_refresh;
    QTimer m_activity;
    SessionSnapshot m_state;
    bool m_busy = false;
    bool m_activityPending = false;
    std::deque<std::function<void(ClientWorker&)>> m_pending;
};

} // namespace console_chat::qt
