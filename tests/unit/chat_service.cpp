#include "console_chat/core/chat_service.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>


using console_chat::core::ChatService;

using console_chat::core::GENERAL_CHAT_NAME;
using console_chat::core::MAX_MESSAGE_LENGTH;
using console_chat::core::MAX_PRIVATE_CHATS_PER_USER;

namespace fs = std::filesystem;

namespace {

bool Contains(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

fs::path MakeTempPath(const std::string& fileName) {
    const auto uniquePart = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() /
        ("console_chat_" + std::to_string(uniquePart) + "_" + fileName);
}

class ChatServiceState : public ::testing::Test {
protected:
    void SetUp() override {
        usersFile = MakeTempPath("users.db");
        chatsFile = MakeTempPath("chats.db");
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove(usersFile, ec);
        fs::remove(chatsFile, ec);
    }

    fs::path usersFile;
    fs::path chatsFile;
};

TEST(ChatService, RegisterLogin) {
    ChatService service;

    EXPECT_TRUE(service.Register("User_1", "user_1", "secret"));
    EXPECT_FALSE(service.Register("User_1 Duplicate", "user_1", "another"));

    EXPECT_TRUE(service.Authenticate("user_1", "secret"));
    EXPECT_FALSE(service.Authenticate("user_1", "wrong"));
    EXPECT_FALSE(service.Authenticate("unknown", "secret"));

    EXPECT_EQ(service.GetUserNameByLogin("user_1"), "User_1");
    EXPECT_TRUE(service.GetUserNameByLogin("unknown").empty());
}

TEST(ChatService, SortedLogins) {
    ChatService service;

    ASSERT_TRUE(service.Register("User_2", "user_2", "pass"));
    ASSERT_TRUE(service.Register("User_1", "user_1", "pass"));
    ASSERT_TRUE(service.Register("User_3", "user_3", "pass"));

    EXPECT_EQ(service.GetAllUserLogins(), (std::vector<std::string>{"user_1", "user_2", "user_3"}));
}

TEST(ChatService, GeneralChat) {
    ChatService service;

    EXPECT_TRUE(service.GetMyChats("").empty());
    EXPECT_TRUE(service.GetMyChats("unknown").empty());

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));

    EXPECT_EQ(service.GetMyChats("user_1"), (std::vector<std::string>{GENERAL_CHAT_NAME}));
}

TEST(ChatService, GeneralMessages) {
    ChatService service;

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));

    EXPECT_FALSE(service.SendMessage("", GENERAL_CHAT_NAME, "Hello"));
    EXPECT_FALSE(service.SendMessage("unknown", GENERAL_CHAT_NAME, "Hello"));
    EXPECT_FALSE(service.SendMessage("user_1", GENERAL_CHAT_NAME, ""));
    EXPECT_FALSE(service.SendMessage("user_1", "missing_chat", "Hello"));

    EXPECT_TRUE(service.SendMessage("user_1", GENERAL_CHAT_NAME, "Hello everyone"));

    const auto messages = service.GetMessages("user_1", GENERAL_CHAT_NAME);
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].Name, "User_1");
    EXPECT_EQ(messages[0].Text, "Hello everyone");
}

TEST(ChatService, MessageLimit) {
    ChatService service;

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));

    EXPECT_TRUE(service.SendMessage("user_1", GENERAL_CHAT_NAME, std::string(MAX_MESSAGE_LENGTH, 'a')));
    EXPECT_FALSE(service.SendMessage("user_1", GENERAL_CHAT_NAME, std::string(MAX_MESSAGE_LENGTH + 1, 'a')));
}

TEST(ChatService, PrivateChat) {
    ChatService service;

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));
    ASSERT_TRUE(service.Register("User_2", "user_2", "secret"));
    ASSERT_TRUE(service.Register("User_4", "user_4", "secret"));

    EXPECT_FALSE(service.CreatePrivateChat("", "user_2", "user_1&2"));
    EXPECT_FALSE(service.CreatePrivateChat("unknown", "user_2", "user_1&2"));
    EXPECT_FALSE(service.CreatePrivateChat("user_1", "user_1", "user_1_self"));
    EXPECT_FALSE(service.CreatePrivateChat("user_1", "unknown", "user_1_unknown"));

    EXPECT_TRUE(service.CreatePrivateChat("user_1", "user_2", "user_1&2"));
    EXPECT_FALSE(service.CreatePrivateChat("user_1", "user_4", "user_1&2"));
    EXPECT_FALSE(service.CreatePrivateChat("user_2", "user_1", "user_1&2_duplicate_pair"));

    const auto aliceChats = service.GetMyChats("user_1");
    const auto bobChats = service.GetMyChats("user_2");
    const auto malloryChats = service.GetMyChats("user_4");

    EXPECT_TRUE(Contains(aliceChats, GENERAL_CHAT_NAME));
    EXPECT_TRUE(Contains(aliceChats, "user_1&2"));
    EXPECT_TRUE(Contains(bobChats, "user_1&2"));
    EXPECT_FALSE(Contains(malloryChats, "user_1&2"));
}

TEST(ChatService, PrivateAccess) {
    ChatService service;

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));
    ASSERT_TRUE(service.Register("User_2", "user_2", "secret"));
    ASSERT_TRUE(service.Register("User_4", "user_4", "secret"));
    ASSERT_TRUE(service.CreatePrivateChat("user_1", "user_2", "user_1&2"));

    EXPECT_FALSE(service.SendMessage("user_4", "user_1&2", "spy message"));
    EXPECT_TRUE(service.GetMessages("user_4", "user_1&2").empty());

    ASSERT_TRUE(service.SendMessage("user_1", "user_1&2", "Private hello"));

    const auto messagesForBob = service.GetMessages("user_2", "user_1&2");
    ASSERT_EQ(messagesForBob.size(), 1u);
    EXPECT_EQ(messagesForBob[0].Name, "User_1");
    EXPECT_EQ(messagesForBob[0].Text, "Private hello");
}

TEST(ChatService, PrivateLimit) {
    ChatService service;

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));

    for (int i = 0; i < MAX_PRIVATE_CHATS_PER_USER; ++i) {
        const auto login = "user" + std::to_string(i);
        ASSERT_TRUE(service.Register("User " + std::to_string(i), std::string(login), "secret"));
        EXPECT_TRUE(service.CreatePrivateChat("user_1", std::string(login), "chat_" + std::to_string(i)));
    }

    ASSERT_TRUE(service.Register("Overflow User", "overflow", "secret"));
    EXPECT_FALSE(service.CreatePrivateChat("user_1", "overflow", "chat_overflow"));
}

TEST_F(ChatServiceState, SaveLoad) {
    ChatService service;

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));
    ASSERT_TRUE(service.Register("User_2", "user_2", "password"));
    ASSERT_TRUE(service.CreatePrivateChat("user_1", "user_2", "user_1&2"));
    ASSERT_TRUE(service.SendMessage("user_1", GENERAL_CHAT_NAME, "Hello general"));
    ASSERT_TRUE(service.SendMessage("user_1", "user_1&2", "Hello User_2"));

    ASSERT_TRUE(service.SaveState(usersFile.string(), chatsFile.string()));

    ChatService loaded;
    ASSERT_TRUE(loaded.LoadState(usersFile.string(), chatsFile.string()));

    EXPECT_TRUE(loaded.Authenticate("user_1", "secret"));
    EXPECT_TRUE(loaded.Authenticate("user_2", "password"));
    EXPECT_EQ(loaded.GetUserNameByLogin("user_1"), "User_1");

    const auto aliceChats = loaded.GetMyChats("user_1");
    EXPECT_TRUE(Contains(aliceChats, GENERAL_CHAT_NAME));
    EXPECT_TRUE(Contains(aliceChats, "user_1&2"));

    const auto generalMessages = loaded.GetMessages("user_2", GENERAL_CHAT_NAME);
    ASSERT_EQ(generalMessages.size(), 1u);
    EXPECT_EQ(generalMessages[0].Name, "User_1");
    EXPECT_EQ(generalMessages[0].Text, "Hello general");

    const auto privateMessages = loaded.GetMessages("user_2", "user_1&2");
    ASSERT_EQ(privateMessages.size(), 1u);
    EXPECT_EQ(privateMessages[0].Name, "User_1");
    EXPECT_EQ(privateMessages[0].Text, "Hello User_2");
}

TEST_F(ChatServiceState, InvalidLoad) {
    ChatService service;

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));
    ASSERT_FALSE(service.LoadState(usersFile.string(), chatsFile.string()));

    EXPECT_TRUE(service.Authenticate("user_1", "secret"));
    EXPECT_EQ(service.GetMyChats("user_1"), (std::vector<std::string>{GENERAL_CHAT_NAME}));
}

} // namespace
