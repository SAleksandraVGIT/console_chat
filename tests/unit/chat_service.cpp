#include "console_chat/core/chat_service.h"
#include "console_chat/storage/file_manager.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>


using console_chat::core::ChatService;

using console_chat::core::GENERAL_CHAT_NAME;
using console_chat::core::MAX_MESSAGE_LENGTH;
using console_chat::core::MAX_PRIVATE_CHATS_PER_USER;
using console_chat::core::ServiceLimits;
using console_chat::storage::FileManager;

namespace fs = std::filesystem;

namespace {

class FailingManager final : public console_chat::storage::IManager {
public:
    FailingManager() {
        console_chat::core::ChatState generalChat;
        generalChat.Name = GENERAL_CHAT_NAME;
        state.Chats.push_back(std::move(generalChat));
    }

    bool Initialize() override {
        return true;
    }

    bool Load(console_chat::core::ServiceState& loaded) override {
        loaded = state;
        return true;
    }

    bool AddUser(const console_chat::core::UserState& user) override {
        if (failAddUser) {
            return false;
        }
        state.Users.push_back(user);
        return true;
    }

    bool AddChat(const console_chat::core::ChatState& chat) override {
        if (failAddChat) {
            return false;
        }
        state.Chats.push_back(chat);
        return true;
    }

    bool AddMessage(
        const std::string& chatName,
        const std::string&,
        const console_chat::core::Message& message,
        const std::size_t) override
    {
        if (failAddMessage) {
            return false;
        }

        const auto chat = std::find_if(
            state.Chats.begin(),
            state.Chats.end(),
            [&chatName](const console_chat::core::ChatState& stored) {
                return stored.Name == chatName;
            });

        if (chat == state.Chats.end()) {
            return false;
        }

        chat->Messages.push_back(message);
        return true;
    }

    bool AddAdminMessage(
        const std::string& chatName,
        const console_chat::core::Message& message,
        const std::size_t) override
    {
        if (failAddMessage) {
            return false;
        }

        const auto chat = std::find_if(
            state.Chats.begin(),
            state.Chats.end(),
            [&chatName](const console_chat::core::ChatState& stored) {
                return stored.Name == chatName;
            });

        if (chat == state.Chats.end()) {
            return false;
        }

        chat->Messages.push_back(message);
        return true;
    }

    bool UpdateUserBan(
        const std::string& login,
        const std::int64_t bannedUntilEpoch,
        const bool bannedForever) override
    {
        const auto user = std::find_if(
            state.Users.begin(),
            state.Users.end(),
            [&login](const console_chat::core::UserState& stored) {
                return stored.Login == login;
            });

        if (user == state.Users.end()) {
            return false;
        }

        user->BannedUntilEpoch = bannedUntilEpoch;
        user->BannedForever = bannedForever;
        return true;
    }

    bool DeletePrivateChatsWithUser(const std::string& login) override {
        const auto newEnd = std::remove_if(
            state.Chats.begin(),
            state.Chats.end(),
            [&login](const console_chat::core::ChatState& chat) {
                return chat.IsPrivate &&
                    (chat.Participants[0] == login || chat.Participants[1] == login);
            });

        state.Chats.erase(newEnd, state.Chats.end());
        return true;
    }

    bool DeleteUser(const std::string& login) override {
        if (failDeleteUser) {
            return false;
        }

        const auto userEnd = std::remove_if(
            state.Users.begin(),
            state.Users.end(),
            [&login](const console_chat::core::UserState& user) {
                return user.Login == login;
            });

        if (userEnd == state.Users.end()) {
            return false;
        }

        state.Users.erase(userEnd, state.Users.end());
        DeletePrivateChatsWithUser(login);
        return true;
    }

    bool Reset() override {
        state = {};
        return true;
    }

    bool failAddUser = false;
    bool failAddChat = false;
    bool failAddMessage = false;
    bool failDeleteUser = false;
    console_chat::core::ServiceState state;
};

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
    EXPECT_FALSE(service.CreatePrivateChat("user_1", "unknown", "user_1_unknown"));

    EXPECT_TRUE(service.CreatePrivateChat("user_1", "user_1", "user_1_self"));
    EXPECT_FALSE(service.CreatePrivateChat("user_1", "user_1", "user_1_self_duplicate"));
    EXPECT_EQ(service.GetPrivateChatName("user_1", "user_1"), "user_1_self");

    EXPECT_TRUE(service.CreatePrivateChat("user_1", "user_2", "user_1&2"));
    EXPECT_FALSE(service.CreatePrivateChat("user_1", "user_4", "user_1&2"));
    EXPECT_FALSE(service.CreatePrivateChat("user_2", "user_1", "user_1&2_duplicate_pair"));
    EXPECT_EQ(service.GetPrivateChatName("user_2", "user_1"), "user_1&2");

    const auto aliceChats = service.GetMyChats("user_1");
    const auto bobChats = service.GetMyChats("user_2");
    const auto malloryChats = service.GetMyChats("user_4");

    EXPECT_TRUE(Contains(aliceChats, GENERAL_CHAT_NAME));
    EXPECT_TRUE(Contains(aliceChats, "user_1_self"));
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

TEST(ChatService, AdminCanReadPrivateChat) {
    ChatService service;

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));
    ASSERT_TRUE(service.Register("User_2", "user_2", "secret"));
    ASSERT_TRUE(service.CreatePrivateChat("user_1", "user_2", "user_1&2"));
    ASSERT_TRUE(service.SendMessage("user_1", "user_1&2", "Private hello"));

    const auto messages = service.GetMessagesForAdmin("user_1&2");
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].Name, "User_1");
    EXPECT_EQ(messages[0].Text, "Private hello");
}

TEST(ChatService, BanUserBlocksLogin) {
    ChatService service;

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));
    ASSERT_TRUE(service.BanUser("user_1", console_chat::core::BanPeriod::OneDay));

    EXPECT_FALSE(service.Authenticate("user_1", "secret"));
    EXPECT_TRUE(service.IsUserBanned("user_1"));

    ASSERT_TRUE(service.UnbanUser("user_1"));
    EXPECT_TRUE(service.Authenticate("user_1", "secret"));
}

TEST(ChatService, ForeverBanDoesNotDeletePrivateChats) {
    ChatService service;

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));
    ASSERT_TRUE(service.Register("User_2", "user_2", "secret"));
    ASSERT_TRUE(service.CreatePrivateChat("user_1", "user_2", "user_1&2"));

    ASSERT_TRUE(service.BanUser("user_1", console_chat::core::BanPeriod::Forever));

    EXPECT_TRUE(Contains(service.GetAllChatNames(), "user_1&2"));
}

TEST(ChatService, DeleteUserAccountRemovesUserAndPrivateChats) {
    ChatService service;

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));
    ASSERT_TRUE(service.Register("User_2", "user_2", "secret"));
    ASSERT_TRUE(service.CreatePrivateChat("user_1", "user_2", "user_1&2"));

    EXPECT_FALSE(service.DeleteUserAccount(""));
    EXPECT_FALSE(service.DeleteUserAccount(console_chat::core::ADMIN_SYSTEM_LOGIN));
    ASSERT_TRUE(service.DeleteUserAccount("user_1"));

    EXPECT_FALSE(service.Authenticate("user_1", "secret"));
    EXPECT_EQ(service.GetAllUserLogins(), (std::vector<std::string>{"user_2"}));
    EXPECT_FALSE(Contains(service.GetAllChatNames(), "user_1&2"));
}

TEST(ChatService, DeleteForeverBannedUsersRemovesOnlyForeverBannedUsers) {
    ChatService service;

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));
    ASSERT_TRUE(service.Register("User_2", "user_2", "secret"));
    ASSERT_TRUE(service.Register("User_3", "user_3", "secret"));
    ASSERT_TRUE(service.CreatePrivateChat("user_1", "user_2", "user_1&2"));
    ASSERT_TRUE(service.CreatePrivateChat("user_2", "user_3", "user_2&3"));
    ASSERT_TRUE(service.BanUser("user_1", console_chat::core::BanPeriod::Forever));
    ASSERT_TRUE(service.BanUser("user_2", console_chat::core::BanPeriod::OneDay));

    size_t deletedCount = 0;
    ASSERT_TRUE(service.DeleteForeverBannedUsers(deletedCount));

    EXPECT_EQ(deletedCount, 1u);
    EXPECT_EQ(service.GetAllUserLogins(), (std::vector<std::string>{"user_2", "user_3"}));
    EXPECT_FALSE(Contains(service.GetAllChatNames(), "user_1&2"));
    EXPECT_TRUE(Contains(service.GetAllChatNames(), "user_2&3"));
    EXPECT_TRUE(service.IsUserBanned("user_2"));
}

TEST(ChatService, AdminPrivateChat) {
    ChatService service;

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));

    EXPECT_TRUE(service.CreateAdminPrivateChat("user_1", "admin_user_1"));
    EXPECT_FALSE(service.CreateAdminPrivateChat("user_1", "duplicate_admin_user_1"));
    EXPECT_TRUE(service.SendAdminMessageToChat("admin_user_1", "Hello from admin"));
    EXPECT_FALSE(service.SendAdminMessageToChat("missing", "Hello"));

    const auto userChats = service.GetMyChats("user_1");
    EXPECT_TRUE(Contains(userChats, "admin_user_1"));

    const auto messages = service.GetMessages("user_1", "admin_user_1");
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].Name, console_chat::core::ADMIN_MESSAGE_NAME);
    EXPECT_EQ(messages[0].Text, "Hello from admin");
}

TEST(ChatService, ImportsAdminPrivateChatWithoutAdminUser) {
    console_chat::core::ServiceState state;
    state.Users.push_back({
        "user_1",
        "User_1",
        console_chat::core::PasswordProtector::Hash("secret")});

    console_chat::core::ChatState chat;
    chat.Name = "admin_user_1";
    chat.IsPrivate = true;
    chat.Participants = {console_chat::core::ADMIN_SYSTEM_LOGIN, "user_1"};
    state.Chats.push_back(std::move(chat));

    ChatService service;
    ASSERT_TRUE(service.ImportState(std::move(state)));
    EXPECT_TRUE(Contains(service.GetMyChats("user_1"), "admin_user_1"));
    EXPECT_TRUE(service.SendAdminMessageToChat("admin_user_1", "Loaded admin chat"));
}

TEST(ChatService, UsesCustomServiceLimits) {
    ServiceLimits userLimits;
    userLimits.MaxUsers = 1;
    ChatService limitedUsers(userLimits);
    EXPECT_TRUE(limitedUsers.Register("User_1", "user_1", "secret"));
    EXPECT_FALSE(limitedUsers.Register("User_2", "user_2", "secret"));

    ServiceLimits chatLimits;
    chatLimits.MaxChats = 1;
    ChatService limitedChats(chatLimits);
    ASSERT_TRUE(limitedChats.Register("User_1", "user_1", "secret"));
    ASSERT_TRUE(limitedChats.Register("User_2", "user_2", "secret"));
    EXPECT_FALSE(limitedChats.CreatePrivateChat("user_1", "user_2", "blocked"));

    ServiceLimits messageLimits;
    messageLimits.MaxMessageLength = 3;
    messageLimits.MaxMessagesPerChat = 1;
    ChatService limitedMessages(messageLimits);
    ASSERT_TRUE(limitedMessages.Register("User_1", "user_1", "secret"));
    EXPECT_FALSE(limitedMessages.SendMessage("user_1", GENERAL_CHAT_NAME, "long"));
    EXPECT_TRUE(limitedMessages.SendMessage("user_1", GENERAL_CHAT_NAME, "one"));
    EXPECT_FALSE(limitedMessages.SendMessage("user_1", GENERAL_CHAT_NAME, "two"));
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

TEST(ChatService, StorageFailureDoesNotChangeMemoryState) {
    FailingManager storage;
    ChatService service(storage);
    ASSERT_TRUE(service.Initialize());

    storage.failAddUser = true;
    EXPECT_FALSE(service.Register("User_1", "user_1", "secret"));
    EXPECT_FALSE(service.Authenticate("user_1", "secret"));

    storage.failAddUser = false;
    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));
    ASSERT_TRUE(service.Register("User_2", "user_2", "secret"));

    storage.failAddChat = true;
    EXPECT_FALSE(service.CreatePrivateChat("user_1", "user_2", "user_1&2"));
    EXPECT_FALSE(Contains(service.GetMyChats("user_1"), "user_1&2"));

    storage.failAddChat = false;
    ASSERT_TRUE(service.CreatePrivateChat("user_1", "user_2", "user_1&2"));

    storage.failAddMessage = true;
    EXPECT_FALSE(service.SendMessage("user_1", "user_1&2", "Not persisted"));
    EXPECT_TRUE(service.GetMessages("user_1", "user_1&2").empty());
}

TEST_F(ChatServiceState, SaveLoad) {
    FileManager storage(usersFile.string(), chatsFile.string());
    ChatService service(storage);
    ASSERT_TRUE(service.Initialize());

    ASSERT_TRUE(service.Register("User_1", "user_1", "secret"));
    ASSERT_TRUE(service.Register("User_2", "user_2", "password"));
    ASSERT_TRUE(service.CreatePrivateChat("user_1", "user_2", "user_1&2"));
    ASSERT_TRUE(service.CreatePrivateChat("user_1", "user_1", "user_1_self"));
    ASSERT_TRUE(service.SendMessage("user_1", GENERAL_CHAT_NAME, "Hello general"));
    ASSERT_TRUE(service.SendMessage("user_1", "user_1&2", "Hello User_2"));
    ASSERT_TRUE(service.SendMessage("user_1", "user_1_self", "Saved self note"));

    FileManager loadedStorage(usersFile.string(), chatsFile.string());
    ChatService loaded(loadedStorage);
    ASSERT_TRUE(loaded.Initialize());

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

    const auto selfMessages = loaded.GetMessages("user_1", "user_1_self");
    ASSERT_EQ(selfMessages.size(), 1u);
    EXPECT_EQ(selfMessages[0].Name, "User_1");
    EXPECT_EQ(selfMessages[0].Text, "Saved self note");
}

TEST_F(ChatServiceState, InvalidLoad) {
    std::ofstream usersOut(usersFile);
    ASSERT_TRUE(usersOut.is_open());
    usersOut << "invalid state\n";
    usersOut.close();

    FileManager storage(usersFile.string(), chatsFile.string());
    ChatService service(storage);
    ASSERT_FALSE(service.Initialize());

    EXPECT_FALSE(service.Authenticate("user_1", "secret"));
}

} // namespace
