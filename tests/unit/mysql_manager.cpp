#include "console_chat/storage/mysql_manager.h"
#include "console_chat/core/base_chat.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <utility>


namespace {

TEST(MySQLManager, HasApplicationDefaults) {
    console_chat::storage::MySQLConfig config;

    EXPECT_EQ(config.Host, "127.0.0.1");
    EXPECT_EQ(config.Port, 3306);
    EXPECT_EQ(config.User, "console_chat");
    EXPECT_TRUE(config.Password.empty());
    EXPECT_EQ(config.Database, "console_chat");
    EXPECT_EQ(config.ConnectTimeout, std::chrono::seconds{5});
    EXPECT_EQ(config.Charset, "utf8mb4");
}

TEST(MySQLManager, RejectsInvalidConfiguration) {
    console_chat::storage::MySQLConfig config;
    config.Port = 0;
    console_chat::storage::MySQLManager manager(std::move(config));

    EXPECT_FALSE(manager.Initialize());
    EXPECT_EQ(manager.GetLastError(), "MySQL port must be greater than zero");
}

TEST(MySQLManager, RejectsOperationsBeforeInitialization) {
    console_chat::storage::MySQLManager manager({});
    console_chat::core::ServiceState state;
    const console_chat::core::UserState user{"login", "name", "h1$hash"};
    const console_chat::core::ChatState chat{"GENERAL"};
    const console_chat::core::Message message{"name", "text"};

    EXPECT_FALSE(manager.Load(state));
    EXPECT_EQ(manager.GetLastError(), "MySQLManager is not initialized");
    EXPECT_FALSE(manager.AddUser(user));
    EXPECT_EQ(manager.GetLastError(), "MySQLManager is not initialized");
    EXPECT_FALSE(manager.AddChat(chat));
    EXPECT_EQ(manager.GetLastError(), "MySQLManager is not initialized");
    EXPECT_FALSE(manager.AddMessage(
        "GENERAL",
        "login",
        message,
        console_chat::core::MAX_MESSAGES_PER_CHAT));
    EXPECT_EQ(manager.GetLastError(), "MySQLManager is not initialized");
}

TEST(MySQLConfig, LoadsFromFile) {
    const auto configPath =
        std::filesystem::temp_directory_path() / "console_chat_mysql_config_test.conf";
    {
        std::ofstream output(configPath);
        output << "host=db.example.test\n"
                  "port=3307\n"
                  "user=chat_user\n"
                  "password=secret=value\n"
                  "database=chat_database\n"
                  "connect_timeout_seconds=12\n"
                  "charset=utf8mb4\n";
    }

    console_chat::storage::MySQLConfig config;
    std::string error;

    ASSERT_TRUE(console_chat::storage::LoadMySQLConfig(
        configPath.string(), config, error)) << error;
    EXPECT_EQ(config.Host, "db.example.test");
    EXPECT_EQ(config.Port, 3307);
    EXPECT_EQ(config.User, "chat_user");
    EXPECT_EQ(config.Password, "secret=value");
    EXPECT_EQ(config.Database, "chat_database");
    EXPECT_EQ(config.ConnectTimeout, std::chrono::seconds{12});
    EXPECT_EQ(config.Charset, "utf8mb4");

    std::filesystem::remove(configPath);
}

TEST(MySQLConfig, RejectsUnknownKey) {
    const auto configPath =
        std::filesystem::temp_directory_path() / "console_chat_mysql_bad_config_test.conf";
    {
        std::ofstream output(configPath);
        output << "unknown=value\n";
    }

    console_chat::storage::MySQLConfig config;
    std::string error;

    EXPECT_FALSE(console_chat::storage::LoadMySQLConfig(
        configPath.string(), config, error));
    EXPECT_EQ(error, "Unknown MySQL config key: unknown");

    std::filesystem::remove(configPath);
}

} // namespace
