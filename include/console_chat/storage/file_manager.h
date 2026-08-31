#pragma once

#include "imanager.h"

#include <string>


namespace console_chat::storage {

class FileManager final : public IManager {
public:
    FileManager(std::string usersFilePath, std::string chatsFilePath);

    bool Initialize() override;

    bool Load(core::ServiceState& state) override;

    bool AddUser(const core::UserState& user) override;
    bool AddChat(const core::ChatState& chat) override;
    bool AddMessage(
        const std::string& chatName,
        const std::string& senderLogin,
        const core::Message& message,
        size_t maxMessagesPerChat) override;

    bool AddAdminMessage(
        const std::string& chatName,
        const core::Message& message,
        size_t maxMessagesPerChat) override;
    bool UpdateUserBan(
        const std::string& login,
        std::int64_t bannedUntilEpoch,
        bool bannedForever) override;
    bool DeletePrivateChatsWithUser(const std::string& login) override;
    bool DeleteUser(const std::string& login) override;

    bool Reset() override;

private:
    bool ReadState(core::ServiceState& state) const;
    bool WriteState(const core::ServiceState& state) const;

    std::string m_usersFilePath;
    std::string m_chatsFilePath;
    core::ServiceState m_state;
    bool m_initialized = false;
};

} // namespace console_chat::storage
