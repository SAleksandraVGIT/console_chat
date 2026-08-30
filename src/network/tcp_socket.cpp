#include "console_chat/network/tcp_socket.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cerrno>
#include <stdexcept>
#include <string>

namespace console_chat::network {

namespace {

#ifdef _WIN32

using NativeSocket = SOCKET;
using SocketLen = int;
constexpr NativeSocket INVALID_NATIVE_SOCKET = INVALID_SOCKET;

NativeSocket ToNative(const std::intptr_t fd) {
    return static_cast<NativeSocket>(fd);
}

std::intptr_t FromNative(const NativeSocket fd) {
    return static_cast<std::intptr_t>(fd);
}

void EnsureWinsockInitialized() {
    static const bool initialized = []() {
        WSADATA wsaData{};
        return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
    }();

    if (!initialized) {
        throw std::runtime_error("Failed to initialize Winsock.");
    }
}

void CloseNativeSocket(const NativeSocket fd) {
    closesocket(fd);
}

bool WasLastNativeReceiveTimeout() {
    const int error = WSAGetLastError();
    return error == WSAETIMEDOUT || error == WSAEWOULDBLOCK;
}

bool SetNativeReceiveTimeout(const NativeSocket fd, const std::chrono::seconds timeout) {
    const auto milliseconds =
        static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count());
    return setsockopt(
        fd,
        SOL_SOCKET,
        SO_RCVTIMEO,
        reinterpret_cast<const char*>(&milliseconds),
        sizeof(milliseconds)) == 0;
}

#else

using SocketLen = socklen_t;

NativeSocket ToNative(const std::intptr_t fd) {
    return static_cast<NativeSocket>(fd);
}

std::intptr_t FromNative(const NativeSocket fd) {
    return static_cast<std::intptr_t>(fd);
}

void EnsureWinsockInitialized() {
}

void CloseNativeSocket(const NativeSocket fd) {
    close(fd);
}

bool WasLastNativeReceiveTimeout() {
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

bool SetNativeReceiveTimeout(const NativeSocket fd, const std::chrono::seconds timeout) {
    timeval value{};
    value.tv_sec = timeout.count();
    value.tv_usec = 0;
    return setsockopt(
        fd,
        SOL_SOCKET,
        SO_RCVTIMEO,
        &value,
        sizeof(value)) == 0;
}

#endif

} // namespace

TcpSocket::~TcpSocket() {
    Close();
}

TcpSocket::TcpSocket(TcpSocket&& other) noexcept
    : m_fd(other.m_fd) {
    other.m_fd = -1;
}

TcpSocket& TcpSocket::operator=(TcpSocket&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    Close();
    m_fd = other.m_fd;
    other.m_fd = -1;
    return *this;
}

void TcpSocket::EnsureCreated() {
    if (m_fd != FromNative(INVALID_NATIVE_SOCKET)) {
        return;
    }

    EnsureWinsockInitialized();

    const NativeSocket created = socket(AF_INET, SOCK_STREAM, 0);
    if (created == INVALID_NATIVE_SOCKET) {
        throw std::runtime_error("Failed to create socket.");
    }

    m_fd = FromNative(created);
}

void TcpSocket::Connect(const std::string& host, const uint16_t port) {
    EnsureCreated();

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        throw std::runtime_error("Invalid server address.");
    }

    if (connect(ToNative(m_fd), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        throw std::runtime_error("Failed to connect to server.");
    }
}

void TcpSocket::BindAndListen(const uint16_t port, const int backlog) {
    EnsureCreated();

    const int opt = 1;
    setsockopt(ToNative(m_fd), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(ToNative(m_fd), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        throw std::runtime_error("Failed to bind server socket.");
    }

    if (listen(ToNative(m_fd), backlog) != 0) {
        throw std::runtime_error("Failed to listen on server socket.");
    }
}

TcpSocket TcpSocket::Accept() const {
    sockaddr_in clientAddr{};
    SocketLen clientLen = static_cast<SocketLen>(sizeof(clientAddr));
    const NativeSocket clientFd = accept(ToNative(m_fd), reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
    return TcpSocket(FromNative(clientFd));
}

bool TcpSocket::SendLine(const std::string& line) const {
    std::string data = line;
    if (data.empty() || data.back() != '\n') {
        data.push_back('\n');
    }

    size_t sent = 0;
    while (sent < data.size()) {
        const auto n = send(ToNative(m_fd), data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }

    return true;
}

bool TcpSocket::RecvLine(std::string& line) const {
    m_lastReceiveTimedOut = false;
    line.clear();
    char ch = '\0';
    while (true) {
        const auto n = recv(ToNative(m_fd), &ch, 1, 0);
        if (n <= 0) {
            m_lastReceiveTimedOut = n < 0 && WasLastNativeReceiveTimeout();
            return false;
        }
        if (ch == '\n') {
            return true;
        }
        line.push_back(ch);
    }
}

std::string TcpSocket::GetPeerAddress() const {
    sockaddr_in peer{};
    SocketLen len = static_cast<SocketLen>(sizeof(peer));
    if (getpeername(ToNative(m_fd), reinterpret_cast<sockaddr*>(&peer), &len) != 0) {
        return "unknown";
    }

    char ip[INET_ADDRSTRLEN] = {};
    if (inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip)) == nullptr) {
        return "unknown";
    }

    return std::string(ip) + ":" + std::to_string(ntohs(peer.sin_port));
}

bool TcpSocket::SetReceiveTimeout(const std::chrono::seconds timeout) {
    if (!IsValid()) {
        return false;
    }

    return SetNativeReceiveTimeout(ToNative(m_fd), timeout);
}

bool TcpSocket::WasLastReceiveTimedOut() const {
    return m_lastReceiveTimedOut;
}

void TcpSocket::Close() {
    if (m_fd != FromNative(INVALID_NATIVE_SOCKET)) {
        CloseNativeSocket(ToNative(m_fd));
        m_fd = FromNative(INVALID_NATIVE_SOCKET);
    }
}

} // namespace console_chat::network
