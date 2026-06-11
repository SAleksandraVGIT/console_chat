#include "console_chat/client/chat_client.h"
#include "console_chat/core/chat_service.h"
#include "console_chat/network/tcp_socket.h"

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
    TestServer(
        uint16_t port,
        int expectedClients,
        fs::path usersFile,
        fs::path chatsFile)
        : m_usersFile(std::move(usersFile))
        , m_chatsFile(std::move(chatsFile))
    {
        m_service.LoadState(m_usersFile.string(), m_chatsFile.string());
        if (!m_service.SaveState(m_usersFile.string(), m_chatsFile.string())) {
            throw std::runtime_error("Failed to initialize test server state.");
        }

        m_server.BindAndListen(port, expectedClients);
        m_acceptThread = std::thread([this, expectedClients]() {
            for (int i = 0; i < expectedClients; ++i) {
                auto client = m_server.Accept();
                if (!client.IsValid()) {
                    continue;
                }

                m_sessions.emplace_back([this, client = std::move(client)]() mutable {
                    HandleClientSession(
                        std::move(client),
                        m_service,
                        m_mutex,
                        m_usersFile.string(),
                        m_chatsFile.string());
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
    ChatService m_service;
    std::mutex m_mutex;
    TcpSocket m_server;
    std::thread m_acceptThread;
    std::vector<std::thread> m_sessions;
    fs::path m_usersFile;
    fs::path m_chatsFile;
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
        EXPECT_EQ(user1.GetCurrentUserName(), "User_1");
        EXPECT_EQ(user1.GetMyChats(), (std::vector<std::string>{GENERAL_CHAT_NAME}));

        ASSERT_TRUE(user1.SendMessage(GENERAL_CHAT_NAME, "Hello over TCP"));

        const auto messages = user2.GetMessages(GENERAL_CHAT_NAME);
        ASSERT_EQ(messages.size(), 1u);
        EXPECT_EQ(messages[0].Name, "User_1");
        EXPECT_EQ(messages[0].Text, "Hello over TCP");
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
