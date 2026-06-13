#pragma once

#include "console_chat/core/service_state.h"

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
        const core::Message& message) = 0;
    virtual bool Reset() = 0;
};

} // namespace console_chat::storage
