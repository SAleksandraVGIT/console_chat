#pragma once

#include "console_chat/core/service_state.h"

#include <cstddef>
#include <cstdint>
#include <string>


namespace console_chat::storage {

class IManager {
public:
    virtual ~IManager() = default;

    virtual bool Initialize() = 0;
    virtual bool Load(core::ServiceState& state) = 0;
    virtual bool AddUser(const core::UserState& user) = 0;
    virtual bool AddChat(const core::ChatState& chat) = 0;
    virtual bool AddMessage(
        const std::string& chatName,
        const std::string& senderLogin,
        const core::Message& message,
        size_t maxMessagesPerChat) = 0;

    virtual bool AddAdminMessage(
        const std::string& chatName,
        const core::Message& message,
        size_t maxMessagesPerChat) = 0;
    virtual bool UpdateUserBan(
        const std::string& login,
        std::int64_t bannedUntilEpoch,
        bool bannedForever) = 0;
    virtual bool DeletePrivateChatsWithUser(const std::string& login) = 0;

    virtual bool Reset() = 0;
};

} // namespace console_chat::storage
