#pragma once

#include "password_protector.h"

#include <string>
#include <utility>


namespace console_chat::core {

class User {
public:
    User(std::string name, std::string passwordHash)
        : m_name(std::move(name))
        , m_passwordHash(std::move(passwordHash))
    {}

    inline const std::string& GetName() const {
        return m_name;
    }

    inline bool CheckPassword(const std::string& pwd) const {
        return PasswordProtector::Verify(pwd, m_passwordHash);
    }

    inline const std::string& GetPasswordHash() const {
        return m_passwordHash;
    }

private:
    std::string m_name;
    std::string m_passwordHash;
};

} // namespace console_chat::core
