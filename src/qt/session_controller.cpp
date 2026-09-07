#include "session_controller.h"
#include "client_worker.h"

namespace console_chat::qt {

SessionController::SessionController(const int refreshIntervalMs, QObject* parent)
    : QObject(parent), m_worker(new ClientWorker) {
    qRegisterMetaType<SessionSnapshot>();
    m_worker->moveToThread(&m_thread);

    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &ClientWorker::snapshotReady, this, [this](const SessionSnapshot& state) {
        if (!state.authenticated) {
            m_activityPending = false;
        }
        m_state = state;
        emit snapshotChanged(m_state);
    });

    connect(m_worker, &ClientWorker::finished, this, [this] {
        m_busy = false;
        if (!m_pending.empty()) {
            auto next = std::move(m_pending.front());
            m_pending.pop_front();
            Dispatch(std::move(next));
            return;
        }
        emit busyChanged(false);
    });

    connect(m_worker, &ClientWorker::error, this, &SessionController::error);

    connect(m_worker, &ClientWorker::notice, this, &SessionController::notice);

    connect(m_worker, &ClientWorker::chatAlreadyExists, this, &SessionController::chatAlreadyExists);

    connect(m_worker, &ClientWorker::chatNameInUse, this, &SessionController::chatNameInUse);

    connect(m_worker, &ClientWorker::messageSent, this, &SessionController::messageSent);

    connect(&m_refresh, &QTimer::timeout, this, [this] {
        if (!m_busy && m_state.authenticated) {
            Dispatch(
                [thisActivity = m_activityPending](ClientWorker& worker) {
                    if (thisActivity) {
                        worker.Activity();
                    }
                    worker.Refresh();
                },
                false);
            m_activityPending = false;
        }
    });

    connect(&m_activity, &QTimer::timeout, this, [this] {
        if (m_activityPending && !m_busy && m_state.authenticated) {
            m_activityPending = false;
            Dispatch([](ClientWorker& worker) {
                worker.Activity();
            }, false);
        }
    });

    m_thread.start();
    m_refresh.start(refreshIntervalMs);
    m_activity.start(1000);
}

SessionController::~SessionController() {
    m_refresh.stop();
    m_activity.stop();
    m_thread.requestInterruption();
    m_thread.quit();
    m_thread.wait();
}

void SessionController::Dispatch(std::function<void(ClientWorker&)> operation, const bool foreground) {
    if (m_busy) {
        m_pending.push_back(std::move(operation));
        emit busyChanged(true);
        return;
    }

    m_busy = true;
    emit busyChanged(foreground);
    auto* worker = m_worker;
    QMetaObject::invokeMethod(worker, [worker, operation = std::move(operation)] {
        worker->Execute([&] {
            operation(*worker);
        });
    }, Qt::QueuedConnection);
}

void SessionController::Login(const LoginRequest& request) {
    Dispatch([request](ClientWorker& worker) {
        worker.Login(request);
    });
}
void SessionController::OpenChat(const QString& name) {
    Dispatch([name](ClientWorker& worker) {
        worker.OpenChat(name);
    });
}
void SessionController::SendMessage(const QString& text) {
    Dispatch([text](ClientWorker& worker) {
        worker.SendMessage(text);
    });
}
void SessionController::CreateChat(const QString& recipient, const QString& name) {
    Dispatch([recipient, name](ClientWorker& worker) {
        worker.CreateChat(recipient, name);
    });
}
void SessionController::Moderate(const UserAction action, const QString& login, const QString& period) {
    Dispatch([action, login, period](ClientWorker& worker) {
        worker.Moderate(action, login, period);
    });
}
void SessionController::DeleteAccount(const QString& confirmation) {
    Dispatch([confirmation](ClientWorker& worker) {
        worker.DeleteAccount(confirmation);
    });
}
void SessionController::DeleteForeverBanned() {
    Dispatch([](ClientWorker& worker) {
        worker.DeleteForeverBanned();
    });
}
void SessionController::Logout() {
    Dispatch([](ClientWorker& worker) {
        worker.Logout();
    });
}
void SessionController::Refresh() {
    Dispatch([](ClientWorker& worker) {
        worker.Activity();
        worker.Refresh();
    });
}
void SessionController::RecordActivity()
{
    m_activityPending = true;
}

} // namespace console_chat::qt
