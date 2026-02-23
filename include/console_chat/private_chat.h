#pragma once

#include "base_chat.h"

#include <string>
#include <utility>


namespace console_chat {

class PrivateChat : public BaseChat {
public:
    PrivateChat(std::string name, std::string user1, std::string user2)
        : BaseChat(std::move(name))
        , m_user1(std::move(user1))
        , m_user2(std::move(user2))
    {}

    bool IsParticipant(const std::string& login) const override;
    bool IsPrivate() const override;


    inline const std::string& User1() const {
        return m_user1;
    }

    inline const std::string& User2() const {
        return m_user2;
    }

private:
    std::string m_user1;
    std::string m_user2;
};

} // namespace console_chat
