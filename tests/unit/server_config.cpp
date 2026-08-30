#include "server_config.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

TEST(ServerConfig, UsesDefaultsWhenFileIsMissing) {
    console_chat::server::ServerConfig config;
    std::string error;

    ASSERT_TRUE(console_chat::server::LoadServerConfig(
        "/tmp/console_chat_missing_server_config.conf",
        config,
        error)) << error;

    EXPECT_FALSE(error.empty());
    EXPECT_EQ(config.Limits.MaxUsers, 0u);
    EXPECT_EQ(config.Limits.MaxChats, console_chat::core::MAX_CHATS_ON_SERVER);
    EXPECT_EQ(config.Limits.MaxPrivateChatsPerUser, console_chat::core::MAX_PRIVATE_CHATS_PER_USER);
    EXPECT_EQ(config.Limits.MaxMessageLength, console_chat::core::MAX_MESSAGE_LENGTH);
    EXPECT_EQ(config.Limits.MaxMessagesPerChat, console_chat::core::MAX_MESSAGES_PER_CHAT);
    EXPECT_EQ(config.ClientIdleTimeout, std::chrono::minutes{15});
}

TEST(ServerConfig, LoadsFromFile) {
    const auto configPath = fs::temp_directory_path() / "console_chat_server_config_test.conf";
    {
        std::ofstream output(configPath);
        output << "max_users=42\n"
                  "client_timeout_seconds=30\n"
                  "max_chats=17\n"
                  "max_private_chats_per_user=4\n"
                  "max_message_length=128\n"
                  "max_messages_per_chat=50\n";
    }

    console_chat::server::ServerConfig config;
    std::string error;

    ASSERT_TRUE(console_chat::server::LoadServerConfig(
        configPath.string(),
        config,
        error)) << error;

    EXPECT_TRUE(error.empty());
    EXPECT_EQ(config.Limits.MaxUsers, 42u);
    EXPECT_EQ(config.ClientIdleTimeout, std::chrono::seconds{30});
    EXPECT_EQ(config.Limits.MaxChats, 17u);
    EXPECT_EQ(config.Limits.MaxPrivateChatsPerUser, 4u);
    EXPECT_EQ(config.Limits.MaxMessageLength, 128u);
    EXPECT_EQ(config.Limits.MaxMessagesPerChat, 50u);

    fs::remove(configPath);
}

TEST(ServerConfig, RejectsInvalidConfig) {
    const auto configPath = fs::temp_directory_path() / "console_chat_server_bad_config_test.conf";
    {
        std::ofstream output(configPath);
        output << "max_chats=0\n";
    }

    console_chat::server::ServerConfig config;
    std::string error;

    EXPECT_FALSE(console_chat::server::LoadServerConfig(
        configPath.string(),
        config,
        error));
    EXPECT_EQ(error, "Server config key must be greater than zero: max_chats");

    fs::remove(configPath);
}

} // namespace
