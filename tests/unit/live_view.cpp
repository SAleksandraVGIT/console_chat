#include "console_chat/client/live_view.h"

#include <gtest/gtest.h>

#ifndef _WIN32
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <clocale>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

class TerminalChild {
public:
    explicit TerminalChild(const std::function<void()>& run) {
        master = posix_openpt(O_RDWR | O_NOCTTY);
        if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0) {
            if (master >= 0) close(master);
            throw std::runtime_error("Failed to create test terminal");
        }

        const std::string slaveName = ptsname(master);
        std::cout.flush();
        child = fork();

        if (child < 0) {
            close(master);
            throw std::runtime_error("Failed to fork terminal test");
        }

        if (child == 0) {
            close(master);
            setsid();
            const int slave = open(slaveName.c_str(), O_RDWR);
            if (slave < 0) _exit(2);
            ioctl(slave, TIOCSCTTY, 0);
            winsize size{12, 32, 0, 0};
            ioctl(slave, TIOCSWINSZ, &size);
            dup2(slave, STDIN_FILENO);
            dup2(slave, STDOUT_FILENO);
            dup2(slave, STDERR_FILENO);
            if (slave > STDERR_FILENO) close(slave);
            setenv("TERM", "xterm", 1);
            std::setlocale(LC_CTYPE, "C.UTF-8");
            termios original{};
            tcgetattr(STDIN_FILENO, &original);

            try {
                run();
            } catch (const std::exception& error) {
                std::cout << "ERROR=" << error.what() << "\n";
            }

            termios restored{};
            tcgetattr(STDIN_FILENO, &restored);
            std::cout << "RESTORED=" << (original.c_lflag == restored.c_lflag &&
                original.c_iflag == restored.c_iflag) << "\n" << std::flush;
            _exit(0);
        }
    }

    ~TerminalChild() {
        kill(child, SIGKILL);
        waitpid(child, nullptr, 0);
        close(master);
    }

    void Send(const std::string& text) {
        ASSERT_EQ(write(master, text.data(), text.size()), static_cast<ssize_t>(text.size()));
    }

    bool WaitFor(const std::string& text) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
        while (output.find(text) == std::string::npos && std::chrono::steady_clock::now() < deadline) {
            pollfd fd{master, POLLIN, 0};
            if (poll(&fd, 1, 50) <= 0) continue;
            char buffer[4096];
            const auto count = read(master, buffer, sizeof(buffer));
            if (count <= 0) break;
            output.append(buffer, count);
        }
        return output.find(text) != std::string::npos;
    }

    int master = -1;
    pid_t child = -1;
    std::string output;
};

TEST(LiveView, RefreshPreservesDraftCursorUtf8AndTerminalMode) {
    TerminalChild terminal([] {
        std::string result;
        int polls = 0;
        {
            console_chat::client::LiveView view(std::chrono::milliseconds{150});
            result = view.ReadLine([&](bool background) {
                if (background) ++polls;
                return polls >= 2 ? "CHAT\nUPDATED\n" : "CHAT\nINITIAL\n";
            }, [] {});
        }
        std::cout << "RESULT=" << result << "\nPOLLED=" << (polls >= 2) << "\n";
    });

    ASSERT_TRUE(terminal.WaitFor("INITIAL"));
    terminal.Send("draft");
    ASSERT_TRUE(terminal.WaitFor("UPDATED"));
    ASSERT_TRUE(terminal.WaitFor("> draft"));
    // Move into the draft after the refresh, then delete a complete UTF-8 character.
    terminal.Send("\033[D\xd0\xaf\177X\r");
    ASSERT_TRUE(terminal.WaitFor("RESULT=drafXt")) << terminal.output;
    EXPECT_TRUE(terminal.WaitFor("POLLED=1"));
    EXPECT_TRUE(terminal.WaitFor("RESTORED=1"));
}

TEST(LiveView, BackgroundFailureRestoresTerminalWithoutReportingActivity) {
    TerminalChild terminal([] {
        int activities = 0;
        int polls = 0;
        try {
            console_chat::client::LiveView view(std::chrono::milliseconds{100});
            view.ReadLine([&](bool background) -> std::string {
                if (background && ++polls == 3) throw std::runtime_error("Disconnected by ADMIN.");
                return "USERS\nINITIAL\n";
            }, [&] { ++activities; });
        } catch (...) {
            std::cout << "ACTIVITIES=" << activities << "\n";
            throw;
        }
    });

    EXPECT_TRUE(terminal.WaitFor("ERROR=Disconnected by ADMIN."));
    EXPECT_TRUE(terminal.WaitFor("ACTIVITIES=0"));
    EXPECT_TRUE(terminal.WaitFor("RESTORED=1"));
    const auto first = terminal.output.find("INITIAL");
    ASSERT_NE(first, std::string::npos);
    EXPECT_EQ(terminal.output.find("INITIAL", first + 1), std::string::npos);
}

TEST(LiveView, ResizeAndScrollingPreserveLongDraft) {
    const std::string draft(100, 'x');
    TerminalChild terminal([] {
        std::string result;
        {
            console_chat::client::LiveView view(std::chrono::milliseconds{100}, true);
            result = view.ReadLine([](bool) {
                return "CHAT\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\nLAST\n";
            }, [] {}, "/0 - exit | /all - history");
        }
        std::cout << "RESULT=" << result << "\n";
    });

    ASSERT_TRUE(terminal.WaitFor("LAST"));
    ASSERT_TRUE(terminal.WaitFor("\033[11;1H\033[2K/0 - exit | /all - history"));
    terminal.Send(draft);
    ASSERT_TRUE(terminal.WaitFor("> xxxxxxxxx"));
    winsize size{6, 16, 0, 0};
    ASSERT_EQ(ioctl(terminal.master, TIOCSWINSZ, &size), 0);
    ASSERT_TRUE(terminal.WaitFor("\033[4;1H\033[2K/0 - exit | /al"));
    ASSERT_TRUE(terminal.WaitFor("\033[5;1H\033[2Kl - history"));
    terminal.Send("\033[5~\033[6~\033[H\033[3~Z\033[F\r");
    ASSERT_TRUE(terminal.WaitFor("RESULT=Z" + draft.substr(1))) << terminal.output;
    EXPECT_TRUE(terminal.WaitFor("RESTORED=1"));
}

TEST(LiveView, CtrlDExitsAndRestoresTerminal) {
    TerminalChild terminal([] {
        console_chat::client::LiveView view(std::chrono::milliseconds{100});
        view.ReadLine([](bool) { return "INITIAL\n"; }, [] {});
    });

    ASSERT_TRUE(terminal.WaitFor("INITIAL"));
    terminal.Send("\004");
    EXPECT_TRUE(terminal.WaitFor("ERROR=Input closed."));
    EXPECT_TRUE(terminal.WaitFor("RESTORED=1"));
}

} // namespace
#endif
