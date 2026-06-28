#include "console_chat/core/password_protector.h"
#include "console_chat/storage/mysql_manager.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <utility>


namespace {

using console_chat::core::ChatState;
using console_chat::core::Message;
using console_chat::core::PasswordProtector;
using console_chat::core::ServiceState;
using console_chat::core::UserState;
using console_chat::storage::LoadMySQLConfig;
using console_chat::storage::MySQLConfig;
using console_chat::storage::MySQLManager;

TEST(MySQLManagerIntegration, ConnectsUsingConfig) {
    const char* configPath = std::getenv("CONSOLE_CHAT_MYSQL_TEST_CONFIG");
    if (!configPath || std::string(configPath).empty()) {
        GTEST_SKIP() << "CONSOLE_CHAT_MYSQL_TEST_CONFIG is not set";
    }

    MySQLConfig config;
    std::string error;
    ASSERT_TRUE(LoadMySQLConfig(configPath, config, error)) << error;

    MySQLManager manager(std::move(config));
    EXPECT_TRUE(manager.Initialize()) << manager.GetLastError();
}

TEST(MySQLManagerIntegration, PersistsAndLoadsServiceState) {
    const char* configPath = std::getenv("CONSOLE_CHAT_MYSQL_TEST_CONFIG");
    if (!configPath || std::string(configPath).empty()) {
        GTEST_SKIP() << "CONSOLE_CHAT_MYSQL_TEST_CONFIG is not set";
    }

    MySQLConfig config;
    std::string error;
    ASSERT_TRUE(LoadMySQLConfig(configPath, config, error)) << error;

    MySQLManager manager(std::move(config));
    ASSERT_TRUE(manager.Initialize()) << manager.GetLastError();
    ASSERT_TRUE(manager.Reset()) << manager.GetLastError();

    const UserState firstUser{
        "user_1", "User One", PasswordProtector::Hash("secret_1")};
    const UserState secondUser{
        "user_2", "User Two", PasswordProtector::Hash("secret_2")};
    ASSERT_TRUE(manager.AddUser(firstUser)) << manager.GetLastError();
    ASSERT_TRUE(manager.AddUser(secondUser)) << manager.GetLastError();

    ChatState generalChat;
    generalChat.Name = "GENERAL";
    ASSERT_TRUE(manager.AddChat(generalChat)) << manager.GetLastError();

    ChatState privateChat;
    privateChat.Name = "PRIVATE_TEST";
    privateChat.IsPrivate = true;
    privateChat.Participants = {firstUser.Login, secondUser.Login};
    ASSERT_TRUE(manager.AddChat(privateChat)) << manager.GetLastError();

    ASSERT_TRUE(manager.AddMessage(
        generalChat.Name,
        firstUser.Login,
        Message{firstUser.Name, "General message"})) << manager.GetLastError();
    ASSERT_TRUE(manager.AddMessage(
        privateChat.Name,
        secondUser.Login,
        Message{secondUser.Name, "Private message"})) << manager.GetLastError();

    ServiceState loaded;
    ASSERT_TRUE(manager.Load(loaded)) << manager.GetLastError();
    ASSERT_EQ(loaded.Users.size(), 2u);
    ASSERT_EQ(loaded.Chats.size(), 2u);
    EXPECT_EQ(loaded.Users[0].Login, firstUser.Login);
    EXPECT_EQ(loaded.Users[1].Login, secondUser.Login);
    EXPECT_EQ(loaded.Chats[0].Name, generalChat.Name);
    ASSERT_EQ(loaded.Chats[0].Messages.size(), 1u);
    EXPECT_EQ(loaded.Chats[0].Messages[0].Name, firstUser.Name);
    EXPECT_EQ(loaded.Chats[0].Messages[0].Text, "General message");
    EXPECT_EQ(loaded.Chats[1].Name, privateChat.Name);
    EXPECT_TRUE(loaded.Chats[1].IsPrivate);
    EXPECT_EQ(loaded.Chats[1].Participants, privateChat.Participants);
    ASSERT_EQ(loaded.Chats[1].Messages.size(), 1u);
    EXPECT_EQ(loaded.Chats[1].Messages[0].Name, secondUser.Name);
    EXPECT_EQ(loaded.Chats[1].Messages[0].Text, "Private message");

    EXPECT_TRUE(manager.Reset()) << manager.GetLastError();
}

} // namespace
