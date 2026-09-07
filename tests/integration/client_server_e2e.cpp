#include "console_chat/client/chat_client.h"
#include "console_chat/core/chat_service.h"
#include "console_chat/network/tcp_socket.h"
#include "console_chat/storage/file_manager.h"

#include "session.h"

#include <gtest/gtest.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <chrono>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>


using console_chat::client::ChatClient;
using console_chat::core::ChatService;
using console_chat::core::GENERAL_CHAT_NAME;
using console_chat::network::TcpSocket;
using console_chat::server::HandleClientSession;
using console_chat::storage::FileManager;

namespace fs = std::filesystem;

namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket INVALID_NATIVE_SOCKET = INVALID_SOCKET;

void CloseSocket(NativeSocket socket) {
    closesocket(socket);
}

void EnsureSocketApiInitialized() {
    static const bool initialized = []() {
        WSADATA wsaData{};
        return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
    }();
    ASSERT_TRUE(initialized);
}
#else

using console_chat::network::INVALID_NATIVE_SOCKET;
using console_chat::network::NativeSocket;

void CloseSocket(NativeSocket socket) {
    close(socket);
}

void EnsureSocketApiInitialized() {
}
#endif

fs::path MakeTempPath(const std::string& fileName) {
    const auto uniquePart = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() /
        ("console_chat_e2e_" + std::to_string(uniquePart) + "_" + fileName);
}

uint16_t FindFreePort() {
    EnsureSocketApiInitialized();

    const NativeSocket probe = socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_NE(probe, INVALID_NATIVE_SOCKET);
    if (probe == INVALID_NATIVE_SOCKET) {
        return 0;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    EXPECT_EQ(bind(probe, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

    socklen_t len = sizeof(addr);
    EXPECT_EQ(getsockname(probe, reinterpret_cast<sockaddr*>(&addr), &len), 0);
    const auto port = ntohs(addr.sin_port);

    CloseSocket(probe);
    return port;
}

class TestServer {
public:
    TestServer(const uint16_t port, const int expectedClients, fs::path usersFile, fs::path chatsFile,
               const std::chrono::seconds timeout = std::chrono::seconds{15})
        : m_usersFile(std::move(usersFile))
        , m_chatsFile(std::move(chatsFile))
        , m_storage(m_usersFile.string(), m_chatsFile.string())
        , m_service(m_storage)
    {
        if (!m_service.Initialize()) {
            throw std::runtime_error("Failed to initialize test server state.");
        }

        m_server.BindAndListen(port, expectedClients);
        m_acceptThread = std::thread([this, expectedClients, timeout]() {
            for (int i = 0; i < expectedClients; ++i) {
                auto client = m_server.Accept();
                if (!client.IsValid()) {
                    continue;
                }

                m_sessions.emplace_back([this, timeout, client = std::move(client)]() mutable {
                    HandleClientSession(
                        std::move(client),
                        m_service,
                        m_mutex,
                        {"operator", "secret", true},
                        &m_registry,
                        timeout);
                });
            }
        });
    }

    ~TestServer() {
        if (m_acceptThread.joinable()) {
            m_acceptThread.join();
        }

        for (auto& session : m_sessions) {
            if (session.joinable()) {
                session.join();
            }
        }
    }

private:
    std::mutex m_mutex;
    console_chat::server::SessionRegistry m_registry;
    TcpSocket m_server;
    std::thread m_acceptThread;
    std::vector<std::thread> m_sessions;
    fs::path m_usersFile;
    fs::path m_chatsFile;
    FileManager m_storage;
    ChatService m_service;
};

class ClientServerE2E : public ::testing::Test {
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

TEST_F(ClientServerE2E, GeneralChat) {
    const auto port = FindFreePort();
    ASSERT_NE(port, 0);

    TestServer server(port, 2, usersFile, chatsFile);

    {
        ChatClient user1("127.0.0.1", port);
        ChatClient user2("127.0.0.1", port);

        ASSERT_TRUE(user1.Register("User_1", "user_1", "secret"));
        ASSERT_TRUE(user2.Register("User_2", "user_2", "secret"));

        ASSERT_TRUE(user1.Authenticate("user_1", "secret"));
        ASSERT_TRUE(user2.Authenticate("user_2", "secret"));

        EXPECT_TRUE(user1.IsAuthenticated());
        EXPECT_EQ(user1.GetCurrentUserLogin(), "user_1");
        EXPECT_EQ(user1.GetCurrentUserName(), "User_1");
        EXPECT_EQ(user1.GetMyChats(), (std::vector<std::string>{GENERAL_CHAT_NAME}));

        ASSERT_TRUE(user1.SendMessage(GENERAL_CHAT_NAME, "Hello over TCP"));

        const auto messages = user2.GetMessages(GENERAL_CHAT_NAME);
        ASSERT_EQ(messages.size(), 1u);
        EXPECT_EQ(messages[0].Name, "User_1");
        EXPECT_EQ(messages[0].Text, "Hello over TCP");
    }
}

TEST_F(ClientServerE2E, PollObservesNewUsersChatsMessagesAndAdminChanges) {
    const auto port = FindFreePort();
    ASSERT_NE(port, 0);
    TestServer server(port, 3, usersFile, chatsFile);
    ChatClient alice("127.0.0.1", port), bob("127.0.0.1", port), admin("127.0.0.1", port);
    ASSERT_TRUE(alice.Register("Alice", "alice", "secret"));
    ASSERT_TRUE(alice.Authenticate("alice", "secret"));
    ASSERT_TRUE(admin.AdminLogin("operator", "secret"));
    EXPECT_EQ(alice.GetAllUserLogins(true), (std::vector<std::string>{"alice"}));
    ASSERT_TRUE(bob.Register("Bob", "bob", "secret"));
    ASSERT_TRUE(bob.Authenticate("bob", "secret"));
    EXPECT_EQ(alice.GetAllUserLogins(true), (std::vector<std::string>{"alice", "bob"}));
    ASSERT_TRUE(bob.CreatePrivateChat("alice", "together"));
    const auto chats = alice.GetMyChats(true);
    EXPECT_NE(std::find(chats.begin(), chats.end(), "together"), chats.end());
    ASSERT_TRUE(bob.SendMessage("together", "new private message"));
    const auto messages = alice.GetMessages("together", true);
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].Text, "new private message");
    EXPECT_EQ(admin.AdminGetMessages("together", true).size(), 1u);
    EXPECT_EQ(admin.AdminGetChats(true).size(), 2u);
    ASSERT_TRUE(admin.AdminSendGeneral("announcement"));
    const auto general = alice.GetMessages("GENERAL", true);
    ASSERT_EQ(general.size(), 1u);
    EXPECT_EQ(general[0].Name, "ADMIN");
    ASSERT_TRUE(bob.DeleteAccount("bob"));
    EXPECT_EQ(alice.GetMyChats(true), (std::vector<std::string>{"GENERAL"}));
    EXPECT_EQ(admin.AdminGetUsers(true).size(), 1u);
}

TEST_F(ClientServerE2E, PollDoesNotPreventIdleDisconnect) {
    const auto port = FindFreePort();
    ASSERT_NE(port, 0);
    TestServer server(port, 1, usersFile, chatsFile, std::chrono::seconds{1});
    ChatClient client("127.0.0.1", port);
    ASSERT_TRUE(client.Register("Alice", "alice", "secret"));
    ASSERT_TRUE(client.Authenticate("alice", "secret"));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    bool disconnected = false;
    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        try {
            client.GetMyChats(true);
        } catch (const std::runtime_error& error) {
            EXPECT_STREQ(error.what(), "Disconnected due to inactivity timeout.");
            disconnected = true;
            break;
        }
    }
    EXPECT_TRUE(disconnected);
}

TEST_F(ClientServerE2E, UserActivityExtendsIdleDeadline) {
    const auto port = FindFreePort();
    ASSERT_NE(port, 0);
    TestServer server(port, 1, usersFile, chatsFile, std::chrono::seconds{2});
    ChatClient client("127.0.0.1", port);
    ASSERT_TRUE(client.Register("Alice", "alice", "secret"));
    ASSERT_TRUE(client.Authenticate("alice", "secret"));
    for (int i = 0; i < 4; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds{650});
        ASSERT_NO_THROW(client.NotifyActivity());
        EXPECT_EQ(client.GetMyChats(true), (std::vector<std::string>{"GENERAL"}));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{2200});
    try {
        client.GetMyChats(true);
        FAIL() << "Idle session remained connected";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "Disconnected due to inactivity timeout.");
    }
}

TEST_F(ClientServerE2E, PollReportsAdminDisconnect) {
    const auto port = FindFreePort();
    ASSERT_NE(port, 0);
    TestServer server(port, 2, usersFile, chatsFile);
    ChatClient client("127.0.0.1", port), admin("127.0.0.1", port);
    ASSERT_TRUE(client.Register("Alice", "alice", "secret"));
    ASSERT_TRUE(client.Authenticate("alice", "secret"));
    ASSERT_TRUE(admin.AdminLogin("operator", "secret"));
    ASSERT_TRUE(admin.AdminKickUser("alice"));
    try {
        client.GetAllUserLogins(true);
        FAIL() << "Kicked session remained connected";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "Disconnected by ADMIN.");
    }
}

TEST_F(ClientServerE2E, PrivateChat) {
    const auto port = FindFreePort();
    ASSERT_NE(port, 0);

    TestServer server(port, 3, usersFile, chatsFile);

    {
        ChatClient user1("127.0.0.1", port);
        ChatClient user2("127.0.0.1", port);
        ChatClient user3("127.0.0.1", port);

        ASSERT_TRUE(user1.Register("User_1", "user_1", "secret"));
        ASSERT_TRUE(user2.Register("User_2", "user_2", "secret"));
        ASSERT_TRUE(user3.Register("User_3", "user_3", "secret"));

        ASSERT_TRUE(user1.Authenticate("user_1", "secret"));
        ASSERT_TRUE(user2.Authenticate("user_2", "secret"));
        ASSERT_TRUE(user3.Authenticate("user_3", "secret"));

        EXPECT_EQ(user1.GetAllUserLogins(), (std::vector<std::string>{"user_1", "user_2", "user_3"}));
        ASSERT_TRUE(user1.CreatePrivateChat("user_2", "user_1&2"));
        ASSERT_TRUE(user1.SendMessage("user_1&2", "Private hello over TCP"));

        const auto messages = user2.GetMessages("user_1&2");
        ASSERT_EQ(messages.size(), 1u);
        EXPECT_EQ(messages[0].Name, "User_1");
        EXPECT_EQ(messages[0].Text, "Private hello over TCP");

        EXPECT_FALSE(user3.SendMessage("user_1&2", "Spy"));
        EXPECT_TRUE(user3.GetMessages("user_1&2").empty());
    }
}

TEST_F(ClientServerE2E, DeleteAccount) {
    const auto port = FindFreePort();
    ASSERT_NE(port, 0);

    TestServer server(port, 1, usersFile, chatsFile);

    {
        ChatClient user1("127.0.0.1", port);

        ASSERT_TRUE(user1.Register("User_1", "user_1", "secret"));
        ASSERT_TRUE(user1.Authenticate("user_1", "secret"));

        EXPECT_FALSE(user1.DeleteAccount("wrong_login"));
        EXPECT_TRUE(user1.IsAuthenticated());

        EXPECT_TRUE(user1.DeleteAccount("user_1"));
        EXPECT_FALSE(user1.IsAuthenticated());
        EXPECT_FALSE(user1.Authenticate("user_1", "secret"));
    }
}

TEST_F(ClientServerE2E, StatePersistsAcrossServerRestart) {
    const auto firstPort = FindFreePort();
    ASSERT_NE(firstPort, 0);

    {
        TestServer server(firstPort, 1, usersFile, chatsFile);
        ChatClient user1("127.0.0.1", firstPort);

        ASSERT_TRUE(user1.Register("User_1", "user_1", "secret"));
        ASSERT_TRUE(user1.Authenticate("user_1", "secret"));
        ASSERT_TRUE(user1.SendMessage(GENERAL_CHAT_NAME, "Message before restart"));
    }

    const auto secondPort = FindFreePort();
    ASSERT_NE(secondPort, 0);

    {
        TestServer server(secondPort, 1, usersFile, chatsFile);
        ChatClient user1("127.0.0.1", secondPort);

        ASSERT_TRUE(user1.Authenticate("user_1", "secret"));

        const auto messages = user1.GetMessages(GENERAL_CHAT_NAME);
        ASSERT_EQ(messages.size(), 1u);
        EXPECT_EQ(messages[0].Name, "User_1");
        EXPECT_EQ(messages[0].Text, "Message before restart");
    }
}

} // namespace
