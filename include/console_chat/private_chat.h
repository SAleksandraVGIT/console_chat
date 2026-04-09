#pragma once

#include "base_chat.h"

#include <string>
#include <utility>
#include <array>
#include <algorithm>

namespace console_chat {

class PrivateChat : public BaseChat {
public:
    PrivateChat(std::string user1, std::string user2)
        : m_users{std::move(user1), std::move(user2)}
    {}

    bool IsParticipant(const std::string& login) const override;
    bool IsPrivate() const override;

    inline bool HasUser(const std::string& login) const {
        return std::find(m_users.begin(), m_users.end(), login) != m_users.end();
    }

private:
    std::array<std::string, 2> m_users;
};

} // namespace console_chat
