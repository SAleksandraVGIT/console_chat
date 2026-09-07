#pragma once

#include <QList>
#include <QMetaType>
#include <QString>

namespace console_chat::qt {

struct LoginRequest {
    QString host = "127.0.0.1";
    int port = 7777;
    QString login;
    QString password;
    QString name;
    bool admin = false;
    bool registration = false;
};

struct ChatItem {
    QString name;
    QString participants;
    bool writable = false;
    bool operator==(const ChatItem&) const = default;
};

struct UserItem {
    QString login;
    QString name;
    QString ban;
    bool operator==(const UserItem&) const = default;
};

struct MessageItem {
    QString name;
    QString text;
    bool operator==(const MessageItem&) const = default;
};

struct SessionSnapshot {
    bool authenticated = false;
    bool admin = false;
    QString login;
    QString name;
    QString selectedChat;
    QList<ChatItem> chats;
    QList<UserItem> users;
    QList<MessageItem> messages;
    bool operator==(const SessionSnapshot&) const = default;
};

enum class UserAction {
    Disconnect,
    Ban,
    Unban
};

} // namespace console_chat::qt

Q_DECLARE_METATYPE(console_chat::qt::SessionSnapshot)
