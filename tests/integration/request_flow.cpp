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
    EXPECT_EQ(Request({"REGISTER", "User_1", "user_1", "secret"}, session), (std::vector<std::string>{"OK"}));
    EXPECT_EQ(Request({"LOGIN", "user_1", "secret"}, session), (std::vector<std::string>{"OK"}));
    EXPECT_EQ(Request({"IS_AUTH"}, session), (std::vector<std::string>{"OK", "1"}));
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
    EXPECT_EQ(Request({"CREATE_PRIVATE", "user_2", "duplicate"}, user1Session), (std::vector<std::string>{"ERR", "create private failed"}));

    EXPECT_EQ(Request({"SEND_MESSAGE", "user_1&2", "Hello User_2"}, user1Session), (std::vector<std::string>{"OK"}));
    EXPECT_EQ(
        Request({"GET_MESSAGES", "user_1&2"}, user2Session),
        (std::vector<std::string>{"OK", "User_1", "Hello User_2"}));

    EXPECT_EQ(Request({"SEND_MESSAGE", "user_1&2", "Spy"}, user3Session), (std::vector<std::string>{"ERR", "send failed"}));
    EXPECT_EQ(Request({"GET_MESSAGES", "user_1&2"}, user3Session), (std::vector<std::string>{"OK"}));
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

TEST_F(RequestFlow, BadRequests) {
    std::string session;

    EXPECT_EQ(Request({}, session), (std::vector<std::string>{"ERR", "empty request"}));
    EXPECT_EQ(Request({"UNKNOWN"}, session), (std::vector<std::string>{"ERR", "bad request"}));
    EXPECT_EQ(Request({"LOGIN", "too_few_args"}, session), (std::vector<std::string>{"ERR", "bad request"}));
}

} // namespace
