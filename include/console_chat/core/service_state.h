#pragma once

#include "base_chat.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>


namespace console_chat::core {

struct UserState {
    std::string Login;
    std::string Name;
    std::string PasswordHash;
    std::int64_t BannedUntilEpoch = 0;
    bool BannedForever = false;
};

struct UserInfo {
    std::string Login;
    std::string Name;
    std::int64_t BannedUntilEpoch = 0;
    bool BannedForever = false;
    bool BannedNow = false;
};

struct ChatInfo {
    std::string Name;
    bool IsPrivate = false;
    std::array<std::string, 2> Participants;
};

struct ChatState {
    std::string Name;
    bool IsPrivate = false;
    std::array<std::string, 2> Participants;
    std::vector<Message> Messages;
};

struct ServiceState {
    std::vector<UserState> Users;
    std::vector<ChatState> Chats;
};

} // namespace console_chat::core
