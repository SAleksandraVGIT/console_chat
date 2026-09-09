#pragma once

#include "session_controller.h"
#include <QMainWindow>

class QAction;
class QLabel;
class QStackedWidget;

namespace console_chat::qt {

class LoginPage;
class ChatPage;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(const LoginRequest& defaults, int refreshIntervalMs, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private:
    void SetSnapshot(const SessionSnapshot& state);
    void CreateChat(const QString& recipient);
    void Moderate(UserAction action, const QString& login);
    void DeleteAccount();
    void CleanupAccounts();

private:
    SessionController m_controller;
    LoginPage* m_login;
    ChatPage* m_chat;
    QStackedWidget* m_pages;
    QLabel* m_identity;
    QLabel* m_status;
    QLabel* m_progress;
    QAction* m_logout;
    QAction* m_deleteAccount;
    QAction* m_refresh;
};

} // namespace console_chat::qt
