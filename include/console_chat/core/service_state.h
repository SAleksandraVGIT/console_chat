#pragma once

#include "base_chat.h"

#include <array>
#include <string>
#include <vector>


namespace console_chat::core {

struct UserState {
    std::string Login;
    std::string Name;
    std::string PasswordHash;
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
