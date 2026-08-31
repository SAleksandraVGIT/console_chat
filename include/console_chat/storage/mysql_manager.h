#pragma once

#include "imanager.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include <mysql.h>

namespace console_chat::storage {

struct MySQLConfig {
    std::string Host = "127.0.0.1";
    std::uint16_t Port = 3306;
    std::string User = "console_chat";
    std::string Password;
    std::string Database = "console_chat";
    std::chrono::seconds ConnectTimeout{5};
    std::string Charset = "utf8mb4";
};

bool LoadMySQLConfig(
    const std::string& filePath,
    MySQLConfig& config,
    std::string& error);

struct MySQLConnectionDeleter {
    void operator()(MYSQL* connection) const noexcept {
        if (connection) {
            mysql_close(connection);
        }
    }
};

using MySQLConnectionPtr = std::unique_ptr<MYSQL, MySQLConnectionDeleter>;

class MySQLManager final : public IManager {
public:
    explicit MySQLManager(MySQLConfig config);

    MySQLManager(const MySQLManager&) = delete;
    MySQLManager(MySQLManager&&) = delete;
    MySQLManager& operator=(const MySQLManager&) = delete;
    MySQLManager& operator=(MySQLManager&&) = delete;

    bool Initialize() override;
    bool Load(core::ServiceState& state) override;
    bool AddUser(const core::UserState& user) override;
    bool AddChat(const core::ChatState& chat) override;

    bool AddMessage(
        const std::string& chatName,
        const std::string& senderLogin,
        const core::Message& message,
        size_t maxMessagesPerChat) override;
    bool AddAdminMessage(
        const std::string& chatName,
        const core::Message& message,
        size_t maxMessagesPerChat) override;

    bool UpdateUserBan(
        const std::string& login,
        std::int64_t bannedUntilEpoch,
        bool bannedForever) override;
    bool DeletePrivateChatsWithUser(const std::string& login) override;
    bool DeleteUser(const std::string& login) override;

    bool Reset() override;

    const std::string& GetLastError() const noexcept;

private:
    bool ValidateConfig();
    bool EnsureInitialized();

private:
    MySQLConfig m_config;
    MySQLConnectionPtr m_connection;
    bool m_initialized = false;
    std::string m_lastError;
};

} // namespace console_chat::storage
