#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace console_chat::network {

using NativeSocket = int;
inline constexpr NativeSocket INVALID_NATIVE_SOCKET = -1;

class TcpSocket {
public:
    TcpSocket() = default;
    explicit TcpSocket(const std::intptr_t fd) : m_fd(fd) {}
    ~TcpSocket();

    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    TcpSocket(TcpSocket&& other) noexcept;
    TcpSocket& operator=(TcpSocket&& other) noexcept;

    void Connect(const std::string& host, const uint16_t port);
    void BindAndListen(const uint16_t port, const int backlog);
    TcpSocket Accept() const;

    bool SendLine(const std::string& line) const;
    bool RecvLine(std::string& line) const;
    std::string GetPeerAddress() const;

    void Close();

    inline  bool IsValid() const {
        return m_fd >= 0;
    }

private:
    void EnsureCreated();

private:
    std::intptr_t m_fd{-1};
};

} // namespace console_chat::network
