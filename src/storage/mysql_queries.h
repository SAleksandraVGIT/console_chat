#pragma once

#include <string_view>


namespace console_chat::storage::mysql_queries {

inline constexpr std::string_view START_TRANSACTION = "START TRANSACTION";

inline constexpr std::string_view SELECT_USERS =
    "SELECT u.login, u.name, p.password_hash, u.banned_until_epoch, u.banned_forever "
    "FROM users AS u "
    "JOIN users_passwords AS p ON p.user_id = u.id "
    "WHERE u.login <> '__admin__' "
    "ORDER BY u.id";

inline constexpr std::string_view SELECT_CHATS =
    "SELECT c.chat_name, first_user.login, second_user.login "
    "FROM chats AS c "
    "LEFT JOIN users AS first_user ON first_user.id = c.first_user_id "
    "LEFT JOIN users AS second_user ON second_user.id = c.second_user_id "
    "ORDER BY c.id";

inline constexpr std::string_view SELECT_MESSAGES =
    "SELECT c.chat_name, u.name, m.message_text "
    "FROM messages AS m "
    "JOIN chats AS c ON c.id = m.chat_id "
    "JOIN users AS u ON u.id = m.sender_id "
    "ORDER BY c.id, m.sent_at, m.id";

inline constexpr std::string_view INSERT_USER =
    "INSERT INTO users (name, login) VALUES (?, ?)";

inline constexpr std::string_view INSERT_PASSWORD =
    "INSERT INTO users_passwords (user_id, password_hash) VALUES (?, ?)";

inline constexpr std::string_view INSERT_GENERAL_CHAT =
    "INSERT INTO chats (chat_name) VALUES (?)";

inline constexpr std::string_view INSERT_PRIVATE_CHAT =
    "INSERT INTO chats (chat_name, first_user_id, second_user_id) "
    "SELECT ?, first_user.id, second_user.id "
    "FROM users AS first_user "
    "CROSS JOIN users AS second_user "
    "WHERE first_user.login = ? AND second_user.login = ?";

inline constexpr std::string_view INSERT_MESSAGE =
    "INSERT INTO messages (message_text, chat_id, sender_id) "
    "SELECT ?, chat.id, sender.id "
    "FROM chats AS chat "
    "CROSS JOIN users AS sender "
    "WHERE chat.chat_name = ? AND sender.login = ? "
    "AND ((chat.first_user_id IS NULL AND chat.second_user_id IS NULL) "
    "OR sender.id IN (chat.first_user_id, chat.second_user_id)) "
    "AND (SELECT COUNT(*) FROM messages AS existing "
    "WHERE existing.chat_id = chat.id) < ?";

inline constexpr std::string_view INSERT_ADMIN_USER =
    "INSERT INTO users (name, login) VALUES ('ADMIN', '__admin__') "
    "ON DUPLICATE KEY UPDATE name = VALUES(name)";

inline constexpr std::string_view INSERT_ADMIN_MESSAGE =
    "INSERT INTO messages (message_text, chat_id, sender_id) "
    "SELECT ?, chat.id, sender.id "
    "FROM chats AS chat "
    "CROSS JOIN users AS sender "
    "WHERE chat.chat_name = ? "
    "AND sender.login = '__admin__' "
    "AND ((chat.first_user_id IS NULL AND chat.second_user_id IS NULL) "
    "OR sender.id IN (chat.first_user_id, chat.second_user_id)) "
    "AND (SELECT COUNT(*) FROM messages AS existing "
    "WHERE existing.chat_id = chat.id) < ?";

inline constexpr std::string_view UPDATE_USER_BAN =
    "UPDATE users SET banned_until_epoch = ?, banned_forever = ? "
    "WHERE login = ? AND login <> '__admin__'";

inline constexpr std::string_view DELETE_MESSAGES_FOR_PRIVATE_CHATS_WITH_USER =
    "DELETE m FROM messages AS m "
    "JOIN chats AS c ON c.id = m.chat_id "
    "JOIN users AS u ON u.login = ? "
    "WHERE c.first_user_id = u.id OR c.second_user_id = u.id";

inline constexpr std::string_view DELETE_PRIVATE_CHATS_WITH_USER =
    "DELETE c FROM chats AS c "
    "JOIN users AS u ON u.login = ? "
    "WHERE c.first_user_id = u.id OR c.second_user_id = u.id";

inline constexpr std::string_view DELETE_MESSAGES_BY_USER =
    "DELETE m FROM messages AS m "
    "JOIN users AS u ON u.id = m.sender_id "
    "WHERE u.login = ?";

inline constexpr std::string_view DELETE_PASSWORD_BY_USER =
    "DELETE p FROM users_passwords AS p "
    "JOIN users AS u ON u.id = p.user_id "
    "WHERE u.login = ?";

inline constexpr std::string_view DELETE_USER_BY_LOGIN =
    "DELETE FROM users WHERE login = ? AND login <> '__admin__'";

inline constexpr std::string_view DELETE_MESSAGES = "DELETE FROM messages";
inline constexpr std::string_view DELETE_CHATS = "DELETE FROM chats";
inline constexpr std::string_view DELETE_PASSWORDS = "DELETE FROM users_passwords";
inline constexpr std::string_view DELETE_USERS = "DELETE FROM users";

inline constexpr std::string_view RESET_STATEMENTS[] = {
    DELETE_MESSAGES,
    DELETE_CHATS,
    DELETE_PASSWORDS,
    DELETE_USERS};

} // namespace console_chat::storage::mysql_queries
