#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>

namespace console_chat::client {

class LiveView {
public:
    explicit LiveView(std::chrono::milliseconds interval, bool followTail = false);

    LiveView(const LiveView&) = delete;
    LiveView& operator=(const LiveView&) = delete;

    ~LiveView();

    bool IsInteractive() const;
    std::string ReadLine(const std::function<std::string(bool)>& snapshot,
                         const std::function<void()>& activity,
                         const std::string& hint = {});

private:
    struct Terminal;
    std::unique_ptr<Terminal> m_terminal;
    std::chrono::milliseconds m_interval;
    bool m_followTail;
};

} // namespace console_chat::client
