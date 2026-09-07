#include "console_chat/client/live_view.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cwchar>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace console_chat::client {
namespace {

enum Key { None = -1, EndOfInput = -2, Left = -3, Right = -4, Home = -5,
           End = -6, Delete = -7, PageUp = -8, PageDown = -9 };

bool Continuation(const char ch) {
    return (static_cast<unsigned char>(ch) & 0xc0) == 0x80;
}

std::size_t Previous(const std::string& text, std::size_t position) {
    if (position > 0) {
        --position;
        while (position > 0 && Continuation(text[position])) {
            --position;
        }
    }
    return position;
}

std::size_t Next(const std::string& text, std::size_t position) {
    if (position < text.size()) {
        ++position;
        while (position < text.size() && Continuation(text[position])) {
            ++position;
        }
    }
    return position;
}

int Width(const std::string_view character) {
    std::mbstate_t state{};
    wchar_t wide{};
    const auto count = std::mbrtowc(&wide, character.data(), character.size(), &state);
    if (count == static_cast<std::size_t>(-1) || count == static_cast<std::size_t>(-2)) {
        return 1;
    }
#ifdef _WIN32
    return (wide >= 0x1100 && (wide <= 0x115f || wide >= 0x2e80)) ? 2 : 1;
#else
    return std::max(0, ::wcwidth(wide));
#endif
}

int Columns(const std::string& text, std::size_t begin, const std::size_t end) {
    int columns = 0;
    while (begin < end) {
        const auto next = Next(text, begin);
        columns += Width(std::string_view(text).substr(begin, next - begin));
        begin = next;
    }
    return columns;
}

std::vector<std::string> Wrap(const std::string& text, const int width) {
    std::vector<std::string> lines(1);
    int columns = 0;
    for (std::size_t i = 0; i < text.size();) {
        const auto next = Next(text, i);
        const auto character = text.substr(i, next - i);
        i = next;
        if (character == "\n") {
            lines.emplace_back();
            columns = 0;
            continue;
        }
        // Server-provided names and messages must not become terminal escape sequences.
        const auto byte = static_cast<unsigned char>(character.front());
        const auto safe = byte < 32 || byte == 127 ? " " : character;
        const int size = Width(safe);
        if (columns + size > width) {
            lines.emplace_back();
            columns = 0;
        }
        lines.back() += safe;
        columns += size;
    }
    if (lines.size() > 1 && lines.back().empty()) {
        lines.pop_back();
    }
    return lines;
}

} // namespace

struct LiveView::Terminal {
    bool interactive = false;
    int rows = 24;
    int columns = 80;
#ifdef _WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD inputMode{}, outputMode{};
    UINT outputCodePage{};
    std::string pending;
#else
    termios saved{};
    std::string escape;
#endif

    Terminal() {
        std::cout.flush();
#ifdef _WIN32
        if (!GetConsoleMode(input, &inputMode) || !GetConsoleMode(output, &outputMode)) {
            return;
        }
        if (!SetConsoleMode(output, outputMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            return;
        }
        if (!SetConsoleMode(input, inputMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT |
                                                 ENABLE_PROCESSED_INPUT))) {
            SetConsoleMode(output, outputMode);
            return;
        }
        outputCodePage = GetConsoleOutputCP();
        SetConsoleOutputCP(CP_UTF8);
#else
        const char* term = std::getenv("TERM");
        if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) ||
            (term && std::string_view(term) == "dumb") || tcgetattr(STDIN_FILENO, &saved) != 0) {
            return;
        }
        auto mode = saved;
        mode.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
        mode.c_iflag &= ~(IXON | ICRNL);
        mode.c_cc[VMIN] = 1;
        mode.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &mode) != 0) {
            return;
        }
#endif
        interactive = true;
        std::cout << "\033[?1049h\033[H" << std::flush;
    }

    ~Terminal() {
        if (!interactive) {
            return;
        }
        std::cout << "\033[?1049l" << std::flush;
#ifdef _WIN32
        SetConsoleMode(input, inputMode);
        SetConsoleMode(output, outputMode);
        SetConsoleOutputCP(outputCodePage);
#else
        tcsetattr(STDIN_FILENO, TCSANOW, &saved);
#endif
    }

    bool Resize() {
        int newRows = rows, newColumns = columns;
#ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO info{};
        if (GetConsoleScreenBufferInfo(output, &info)) {
            newRows = info.srWindow.Bottom - info.srWindow.Top + 1;
            newColumns = info.srWindow.Right - info.srWindow.Left + 1;
        }
#else
        winsize size{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_row && size.ws_col) {
            newRows = size.ws_row;
            newColumns = size.ws_col;
        }
#endif
        newRows = std::max(2, newRows);
        newColumns = std::max(4, newColumns);
        const bool changed = rows != newRows || columns != newColumns;
        rows = newRows;
        columns = newColumns;
        return changed;
    }

    int ReadKey() {
#ifdef _WIN32
        if (!pending.empty()) {
            const auto ch = static_cast<unsigned char>(pending.front());
            pending.erase(0, 1);
            return ch;
        }
        const auto result = WaitForSingleObject(input, 50);
        if (result == WAIT_TIMEOUT) return None;
        if (result != WAIT_OBJECT_0) return EndOfInput;
        INPUT_RECORD record{};
        DWORD count{};
        if (!ReadConsoleInputW(input, &record, 1, &count)) return EndOfInput;
        if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) return None;
        const auto& key = record.Event.KeyEvent;
        switch (key.wVirtualKeyCode) {
            case VK_LEFT: return Left;
            case VK_RIGHT: return Right;
            case VK_HOME: return Home;
            case VK_END: return End;
            case VK_DELETE: return Delete;
            case VK_PRIOR: return PageUp;
            case VK_NEXT: return PageDown;
        }
        if (!key.uChar.UnicodeChar) return None;
        char bytes[4]{};
        const int size = WideCharToMultiByte(CP_UTF8, 0, &key.uChar.UnicodeChar, 1,
                                             bytes, sizeof(bytes), nullptr, nullptr);
        if (size <= 0) return None;
        pending.assign(bytes + 1, size - 1);
        return static_cast<unsigned char>(bytes[0]);
#else
        pollfd descriptor{STDIN_FILENO, POLLIN, 0};
        const int ready = poll(&descriptor, 1, 50);
        if (ready < 0) {
            if (errno == EINTR) return None;
            return EndOfInput;
        }
        if (ready == 0) {
            escape.clear();
            return None;
        }
        unsigned char ch{};
        if (read(STDIN_FILENO, &ch, 1) != 1) return EndOfInput;
        if (ch == 27) {
            escape = "\033";
            return None;
        }
        if (!escape.empty()) {
            escape += static_cast<char>(ch);
            if (escape == "\033[" || escape == "\033O") return None;
            if (ch >= 0x20 && ch <= 0x3f && escape.size() < 32) return None;
            const auto sequence = std::exchange(escape, {});
            if (sequence == "\033[D") return Left;
            if (sequence == "\033[C") return Right;
            if (sequence == "\033[H" || sequence == "\033OH" || sequence == "\033[1~") return Home;
            if (sequence == "\033[F" || sequence == "\033OF" || sequence == "\033[4~") return End;
            if (sequence == "\033[3~") return Delete;
            if (sequence == "\033[5~") return PageUp;
            if (sequence == "\033[6~") return PageDown;
            return None;
        }
        return ch;
#endif
    }

    void Render(const std::string& content, const std::string& inputText,
                const std::size_t cursor, std::size_t& scroll, const bool followTail,
                const std::string& hint) {
        const int width = columns - 1;
        const auto lines = Wrap(content, width);
        const auto hints = hint.empty() ? std::vector<std::string>{} : Wrap(hint, width);
        const auto available = static_cast<std::size_t>(std::max(0, rows - 2));
        const auto hintHeight = std::min(hints.size(), available);
        const auto height = available - hintHeight;
        const auto bodySize = lines.size() - 1;
        const auto maximum = bodySize > height ? bodySize - height : 0;
        scroll = std::min(scroll, maximum);
        const auto start = followTail ? maximum - scroll : scroll;
        std::string frame = "\033[1;1H\033[2K" + lines.front();
        for (std::size_t row = 0; row < height; ++row) {
            frame += "\033[" + std::to_string(row + 2) + ";1H\033[2K";
            if (start + row < bodySize) frame += lines[start + row + 1];
        }
        for (std::size_t row = 0; row < hintHeight; ++row) {
            frame += "\033[" + std::to_string(height + row + 2) + ";1H\033[2K" + hints[row];
        }
        std::size_t begin = cursor;
        int beforeCursor = 0;
        while (begin > 0) {
            const auto previous = Previous(inputText, begin);
            const int size = Columns(inputText, previous, begin);
            if (beforeCursor + size > width - 3) break;
            beforeCursor += size;
            begin = previous;
        }
        auto end = cursor;
        int visibleWidth = beforeCursor;
        while (end < inputText.size()) {
            const auto next = Next(inputText, end);
            const int size = Columns(inputText, end, next);
            if (visibleWidth + size > width - 2) break;
            visibleWidth += size;
            end = next;
        }
        frame += "\033[" + std::to_string(rows) + ";1H\033[2K> " + inputText.substr(begin, end - begin);
        frame += "\033[" + std::to_string(rows) + ";" + std::to_string(beforeCursor + 3) + "H";
        std::cout << frame << std::flush;
    }
};

LiveView::LiveView(const std::chrono::milliseconds interval, const bool followTail)
    : m_terminal(std::make_unique<Terminal>()), m_interval(interval), m_followTail(followTail) {}

LiveView::~LiveView() = default;

bool LiveView::IsInteractive() const { return m_terminal->interactive; }

std::string LiveView::ReadLine(const std::function<std::string(bool)>& snapshot,
                             const std::function<void()>& activity,
                             const std::string& hint) {
    auto content = snapshot(false);
    std::string input;
    if (!IsInteractive()) {
        std::cout << content;
        if (!hint.empty()) std::cout << hint << '\n';
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, input)) throw std::runtime_error("Input closed.");
        return input;
    }

    using Clock = std::chrono::steady_clock;
    auto refreshAt = Clock::now() + m_interval;
    auto activityAt = Clock::now();
    std::size_t cursor = 0, scroll = 0;
    bool dirty = true, pendingActivity = false;
    while (true) {
        if (pendingActivity && Clock::now() >= activityAt) {
            activity();
            pendingActivity = false;
            activityAt = Clock::now() + std::chrono::seconds{1};
        }
        if (Clock::now() >= refreshAt) {
            auto updated = snapshot(true);
            dirty = dirty || content != updated;
            content = std::move(updated);
            refreshAt = Clock::now() + m_interval;
        }
        dirty = m_terminal->Resize() || dirty;
        if (dirty) {
            m_terminal->Render(content, input, cursor, scroll, m_followTail, hint);
            dirty = false;
        }
        const int key = m_terminal->ReadKey();
        if (key == EndOfInput || key == 3 || (key == 4 && input.empty())) {
            throw std::runtime_error("Input closed.");
        }
        if (key == None) continue;
        pendingActivity = true;
        dirty = true;
        if (key == '\r' || key == '\n') {
            activity();
            return input;
        }
        if (key == Left) cursor = Previous(input, cursor);
        else if (key == Right) cursor = Next(input, cursor);
        else if (key == Home || key == 1) cursor = 0;
        else if (key == End || key == 5) cursor = input.size();
        else if (key == 8 || key == 127) {
            const auto previous = Previous(input, cursor);
            input.erase(previous, cursor - previous);
            cursor = previous;
        } else if (key == Delete || key == 4) {
            input.erase(cursor, Next(input, cursor) - cursor);
        } else if (key == 21) {
            input.erase(0, cursor);
            cursor = 0;
        } else if (key == PageUp || key == PageDown) {
            const auto page = static_cast<std::size_t>(m_terminal->rows - 1);
            if ((key == PageUp) == m_followTail) scroll += page;
            else scroll -= std::min(scroll, page);
        } else if (key >= 32 && key <= 255) {
            input.insert(cursor++, 1, static_cast<char>(key));
        }
    }
}

} // namespace console_chat::client
