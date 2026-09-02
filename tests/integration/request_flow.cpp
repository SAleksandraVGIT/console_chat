#include "console_chat/core/chat_service.h"
#include "console_chat/storage/file_manager.h"

#include "protocol.h"
#include "request_router.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>


using console_chat::core::ChatService;
using console_chat::core::GENERAL_CHAT_NAME;
using console_chat::server::HandleRequest;
using console_chat::server::Join;
using console_chat::server::AdminCredentials;
using console_chat::server::RequestContext;
using console_chat::server::SessionController;
using console_chat::server::Split;
using console_chat::storage::FileManager;

namespace fs = std::filesystem;

namespace {

fs::path MakeTempPath(const std::string& fileName) {
    const auto uniquePart = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() /
        ("console_chat_integration_" + std::to_string(uniquePart) + "_" + fileName);
}

class RequestFlow : public ::testing::Test {
protected:
    void SetUp() override {
        usersFile = MakeTempPath("users.db");
        chatsFile = MakeTempPath("chats.db");
        storage = std::make_unique<FileManager>(usersFile.string(), chatsFile.string());
        service = std::make_unique<ChatService>(*storage);
        ASSERT_TRUE(service->Initialize());
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove(usersFile, ec);
        fs::remove(chatsFile, ec);
    }

    std::vector<std::string> Request(
        const std::vector<std::string>& req,
        std::string& currentLogin)
    {
        return HandleRequest(req, *service, currentLogin);
    }

    fs::path usersFile;
    fs::path chatsFile;
    std::unique_ptr<FileManager> storage;
    std::unique_ptr<ChatService> service;
};

class FakeSessionController final : public SessionController {
public:
    bool KickUser(const std::string& login) override {
        kickedLogins.push_back(login);
        return true;
    }

    std::vector<std::string> kickedLogins;
};

TEST(Protocol, SplitJoin) {
    EXPECT_EQ(
        Split("REGISTER\tUser_1\tuser_1\tsecret", '\t'),
        (std::vector<std::string>{"REGISTER", "User_1", "user_1", "secret"}));

    EXPECT_EQ(
        Join({"OK", "GENERAL"}, '\t'),
        "OK\tGENERAL\n");
}

TEST_F(RequestFlow, GeneralChat) {
    std::string session;

    EXPECT_EQ(Request({"IS_AUTH"}, session), (std::vector<std::string>{"OK", "0"}));
    EXPECT_EQ(
        Request({"REGISTER", "Fake Admin", console_chat::core::ADMIN_MESSAGE_NAME, "secret"}, session),
        (std::vector<std::string>{"ERR", "register failed"}));
    EXPECT_EQ(
        Request({"REGISTER", "Fake Admin Lower", "admin", "secret"}, session),
        (std::vector<std::string>{"ERR", "register failed"}));
    EXPECT_EQ(
        Request({"REGISTER", "System Admin", console_chat::core::ADMIN_SYSTEM_LOGIN, "secret"}, session),
        (std::vector<std::string>{"ERR", "register failed"}));
    EXPECT_EQ(Request({"REGISTER", "User_1", "user_1", "secret"}, session), (std::vector<std::string>{"OK"}));
    EXPECT_EQ(Request({"LOGIN", "user_1", "secret"}, session), (std::vector<std::string>{"OK"}));
    EXPECT_EQ(Request({"IS_AUTH"}, session), (std::vector<std::string>{"OK", "1"}));
    EXPECT_EQ(Request({"CUR_LOGIN"}, session), (std::vector<std::string>{"OK", "user_1"}));
    EXPECT_EQ(Request({"CUR_USER"}, session), (std::vector<std::string>{"OK", "User_1"}));
    EXPECT_EQ(Request({"GET_MY_CHATS"}, session), (std::vector<std::string>{"OK", GENERAL_CHAT_NAME}));

    EXPECT_EQ(Request({"SEND_MESSAGE", GENERAL_CHAT_NAME, "Hello general"}, session), (std::vector<std::string>{"OK"}));
    EXPECT_EQ(
        Request({"GET_MESSAGES", GENERAL_CHAT_NAME}, session),
        (std::vector<std::string>{"OK", "User_1", "Hello general"}));

    EXPECT_EQ(Request({"LOGOUT"}, session), (std::vector<std::string>{"OK"}));
    EXPECT_EQ(Request({"IS_AUTH"}, session), (std::vector<std::string>{"OK", "0"}));
}

TEST_F(RequestFlow, PrivateChat) {
    std::string user1Session;
    std::string user2Session;
    std::string user3Session;

    ASSERT_EQ(Request({"REGISTER", "User_1", "user_1", "secret"}, user1Session), (std::vector<std::string>{"OK"}));
    ASSERT_EQ(Request({"REGISTER", "User_2", "user_2", "secret"}, user2Session), (std::vector<std::string>{"OK"}));
    ASSERT_EQ(Request({"REGISTER", "User_3", "user_3", "secret"}, user3Session), (std::vector<std::string>{"OK"}));

    ASSERT_EQ(Request({"LOGIN", "user_1", "secret"}, user1Session), (std::vector<std::string>{"OK"}));
    ASSERT_EQ(Request({"LOGIN", "user_2", "secret"}, user2Session), (std::vector<std::string>{"OK"}));
    ASSERT_EQ(Request({"LOGIN", "user_3", "secret"}, user3Session), (std::vector<std::string>{"OK"}));

    EXPECT_EQ(
        Request({"GET_ALL_USERS"}, user1Session),
        (std::vector<std::string>{"OK", "user_1", "user_2", "user_3"}));

    EXPECT_EQ(Request({"CREATE_PRIVATE", "user_2", "user_1&2"}, user1Session), (std::vector<std::string>{"OK"}));
    EXPECT_EQ(
        Request({"CREATE_PRIVATE", "user_2", "duplicate"}, user1Session),
        (std::vector<std::string>{"ERR", "chat already exists", "user_1&2"}));
    EXPECT_EQ(Request({"CREATE_PRIVATE", "user_1", "user_1_self"}, user1Session), (std::vector<std::string>{"OK"}));
    EXPECT_EQ(
        Request({"CREATE_PRIVATE", "user_1", "duplicate_self"}, user1Session),
        (std::vector<std::string>{"ERR", "chat already exists", "user_1_self"}));

    EXPECT_EQ(Request({"SEND_MESSAGE", "user_1&2", "Hello User_2"}, user1Session), (std::vector<std::string>{"OK"}));
    EXPECT_EQ(Request({"SEND_MESSAGE", "user_1_self", "Note to self"}, user1Session), (std::vector<std::string>{"OK"}));
    EXPECT_EQ(
        Request({"GET_MESSAGES", "user_1&2"}, user2Session),
        (std::vector<std::string>{"OK", "User_1", "Hello User_2"}));
    EXPECT_EQ(
        Request({"GET_MESSAGES", "user_1_self"}, user1Session),
        (std::vector<std::string>{"OK", "User_1", "Note to self"}));

    EXPECT_EQ(Request({"SEND_MESSAGE", "user_1&2", "Spy"}, user3Session), (std::vector<std::string>{"ERR", "send failed"}));
    EXPECT_EQ(Request({"GET_MESSAGES", "user_1&2"}, user3Session), (std::vector<std::string>{"OK"}));
}

TEST_F(RequestFlow, AdminCanReadPrivateChatAndBanUser) {
    std::string user1Session;
    std::string user2Session;
    ASSERT_EQ(Request({"REGISTER", "User_1", "user_1", "secret"}, user1Session), (std::vector<std::string>{"OK"}));
    ASSERT_EQ(Request({"REGISTER", "User_2", "user_2", "secret"}, user2Session), (std::vector<std::string>{"OK"}));
    ASSERT_EQ(Request({"LOGIN", "user_1", "secret"}, user1Session), (std::vector<std::string>{"OK"}));
    ASSERT_EQ(Request({"CREATE_PRIVATE", "user_2", "user_1&2"}, user1Session), (std::vector<std::string>{"OK"}));
    ASSERT_EQ(Request({"SEND_MESSAGE", "user_1&2", "Private message"}, user1Session), (std::vector<std::string>{"OK"}));

    RequestContext adminContext;
    FakeSessionController sessions;
    const AdminCredentials credentials{"admin", "secret", true};

    EXPECT_EQ(
        HandleRequest({"ADMIN_GET_MESSAGES", "user_1&2"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"ERR", "access denied"}));
    EXPECT_EQ(
        HandleRequest({"ADMIN_LOGIN", "admin", "secret"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"OK"}));
    EXPECT_EQ(
        HandleRequest({"ADMIN_GET_MESSAGES", "user_1&2"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"OK", "User_1", "Private message"}));
    EXPECT_EQ(
        HandleRequest({"ADMIN_BAN_USER", "user_1", "forever"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"OK"}));

    EXPECT_EQ(sessions.kickedLogins, (std::vector<std::string>{"user_1"}));
    std::string bannedSession;
    const auto bannedLogin = HandleRequest({"LOGIN", "user_1", "secret"}, *service, bannedSession);
    ASSERT_GE(bannedLogin.size(), 5u);
    EXPECT_EQ(bannedLogin[0], "ERR");
    EXPECT_EQ(bannedLogin[1], "banned");
    EXPECT_EQ(bannedLogin[2], "FOREVER");

    EXPECT_EQ(
        HandleRequest({"ADMIN_UNBAN_USER", "user_1"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"OK"}));
    EXPECT_EQ(
        HandleRequest({"LOGIN", "user_1", "secret"}, *service, bannedSession),
        (std::vector<std::string>{"OK"}));
    EXPECT_EQ(
        HandleRequest({"ADMIN_BAN_USER", "user_1", "forever"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"OK"}));
    EXPECT_EQ(
        HandleRequest({"ADMIN_GET_CHATS"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{
            "OK",
            GENERAL_CHAT_NAME,
            "GENERAL",
            "",
            "",
            "user_1&2",
            "PRIVATE",
            "user_1",
            "user_2"}));
    EXPECT_EQ(
        HandleRequest({"ADMIN_DELETE_FOREVER_BANNED_USERS"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"OK", "1"}));
    EXPECT_EQ(
        HandleRequest({"ADMIN_GET_CHATS"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"OK", GENERAL_CHAT_NAME, "GENERAL", "", ""}));
    EXPECT_EQ(
        HandleRequest({"ADMIN_DELETE_FOREVER_BANNED_USERS"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"OK", "0"}));
}

TEST_F(RequestFlow, AdminCanCreateOwnPrivateChatAndSendMessage) {
    std::string userSession;
    ASSERT_EQ(Request({"REGISTER", "User_1", "user_1", "secret"}, userSession), (std::vector<std::string>{"OK"}));

    RequestContext adminContext;
    FakeSessionController sessions;
    const AdminCredentials credentials{"admin", "secret", true};

    ASSERT_EQ(
        HandleRequest({"ADMIN_LOGIN", "admin", "secret"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"OK"}));
    EXPECT_EQ(
        HandleRequest({"ADMIN_CREATE_PRIVATE", "user_1", "admin_user_1"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"OK"}));
    EXPECT_EQ(
        HandleRequest({"ADMIN_CREATE_PRIVATE", "user_1", "duplicate_admin_user_1"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"ERR", "chat already exists", "admin_user_1"}));
    EXPECT_EQ(
        HandleRequest({"ADMIN_GET_CHATS"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{
            "OK",
            GENERAL_CHAT_NAME,
            "GENERAL",
            "",
            "",
            "admin_user_1",
            "PRIVATE",
            console_chat::core::ADMIN_SYSTEM_LOGIN,
            "user_1"}));
    EXPECT_EQ(
        HandleRequest({"ADMIN_SEND_CHAT", "admin_user_1", "Hello from admin"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"OK"}));

    ASSERT_EQ(Request({"LOGIN", "user_1", "secret"}, userSession), (std::vector<std::string>{"OK"}));
    EXPECT_EQ(
        Request({"GET_MY_CHATS"}, userSession),
        (std::vector<std::string>{"OK", GENERAL_CHAT_NAME, "admin_user_1"}));
    EXPECT_EQ(
        Request({"GET_MESSAGES", "admin_user_1"}, userSession),
        (std::vector<std::string>{"OK", console_chat::core::ADMIN_MESSAGE_NAME, "Hello from admin"}));
}

TEST_F(RequestFlow, StateReload) {
    std::string session;

    ASSERT_EQ(Request({"REGISTER", "User_1", "user_1", "secret"}, session), (std::vector<std::string>{"OK"}));
    ASSERT_EQ(Request({"LOGIN", "user_1", "secret"}, session), (std::vector<std::string>{"OK"}));
    ASSERT_EQ(Request({"SEND_MESSAGE", GENERAL_CHAT_NAME, "Saved message"}, session), (std::vector<std::string>{"OK"}));

    FileManager loadedStorage(usersFile.string(), chatsFile.string());
    ChatService loaded(loadedStorage);
    ASSERT_TRUE(loaded.Initialize());

    std::string loadedSession;
    EXPECT_EQ(
        HandleRequest({"LOGIN", "user_1", "secret"}, loaded, loadedSession),
        (std::vector<std::string>{"OK"}));
    EXPECT_EQ(
        HandleRequest({"GET_MESSAGES", GENERAL_CHAT_NAME}, loaded, loadedSession),
        (std::vector<std::string>{"OK", "User_1", "Saved message"}));
}

TEST_F(RequestFlow, DeleteAccountRequiresConfirmationAndLogsOut) {
    std::string session;
    ASSERT_EQ(Request({"REGISTER", "User_1", "user_1", "secret"}, session), (std::vector<std::string>{"OK"}));
    ASSERT_EQ(Request({"REGISTER", "User_2", "user_2", "secret"}, session), (std::vector<std::string>{"OK"}));

    EXPECT_EQ(
        Request({"DELETE_ACCOUNT", "user_1"}, session),
        (std::vector<std::string>{"ERR", "access denied"}));

    ASSERT_EQ(Request({"LOGIN", "user_1", "secret"}, session), (std::vector<std::string>{"OK"}));
    ASSERT_EQ(Request({"CREATE_PRIVATE", "user_2", "user_1&2"}, session), (std::vector<std::string>{"OK"}));

    EXPECT_EQ(
        Request({"DELETE_ACCOUNT", "wrong_login"}, session),
        (std::vector<std::string>{"ERR", "confirmation mismatch"}));
    EXPECT_EQ(Request({"IS_AUTH"}, session), (std::vector<std::string>{"OK", "1"}));

    EXPECT_EQ(Request({"DELETE_ACCOUNT", "user_1"}, session), (std::vector<std::string>{"OK"}));
    EXPECT_EQ(Request({"IS_AUTH"}, session), (std::vector<std::string>{"OK", "0"}));
    EXPECT_EQ(Request({"LOGIN", "user_1", "secret"}, session), (std::vector<std::string>{"ERR", "auth failed"}));
    EXPECT_EQ(
        Request({"GET_ALL_USERS"}, session),
        (std::vector<std::string>{"OK", "user_2"}));

    RequestContext adminContext;
    FakeSessionController sessions;
    const AdminCredentials credentials{"admin", "secret", true};
    ASSERT_EQ(
        HandleRequest({"ADMIN_LOGIN", "admin", "secret"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"OK"}));
    EXPECT_EQ(
        HandleRequest({"ADMIN_GET_CHATS"}, *service, adminContext, credentials, &sessions),
        (std::vector<std::string>{"OK", GENERAL_CHAT_NAME, "GENERAL", "", ""}));
}

TEST_F(RequestFlow, BadRequests) {
    std::string session;

    EXPECT_EQ(Request({}, session), (std::vector<std::string>{"ERR", "empty request"}));
    EXPECT_EQ(Request({"UNKNOWN"}, session), (std::vector<std::string>{"ERR", "bad request"}));
    EXPECT_EQ(Request({"LOGIN", "too_few_args"}, session), (std::vector<std::string>{"ERR", "bad request"}));
}

} // namespace
