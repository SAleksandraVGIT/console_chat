#pragma once

#include "session_types.h"
#include <QHash>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;
class QTabWidget;
class QTextBrowser;

namespace console_chat::qt {

class ChatPage final : public QWidget {
    Q_OBJECT
public:
    explicit ChatPage(QWidget* parent = nullptr);
    void SetSnapshot(const SessionSnapshot& snapshot);
    void SetBusy(bool busy);
    void MessageSent(const QString& chat, const QString& text);
    QString SelectedUser() const;

signals:
    void chatSelected(const QString& name);
    void sendRequested(const QString& text);
    void createRequested(const QString& recipient);
    void moderateRequested(UserAction action, const QString& login);
    void cleanupRequested();

private:
    void FilterChats();
    void UpdateControls();
    void SubmitMessage();

private:
    SessionSnapshot m_state;
    QHash<QString, QString> m_drafts;
    bool m_busy = false;
    QTabWidget* m_tabs;
    QListWidget* m_chats;
    QLineEdit* m_filter;
    QLabel* m_title;
    QLabel* m_participants;
    QLabel* m_access;
    QTextBrowser* m_messages;
    QLineEdit* m_composer;
    QPushButton* m_send;
    QPushButton* m_create;
    QTableWidget* m_users;
    QPushButton* m_userChat;
    QWidget* m_adminActions;
    QPushButton* m_disconnect;
    QPushButton* m_ban;
    QPushButton* m_unban;
    QPushButton* m_cleanup;
};

} // namespace console_chat::qt
