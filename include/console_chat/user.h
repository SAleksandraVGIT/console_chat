#pragma once

#include <string>
#include <utility>

namespace console_chat {

class User {
public:
    User(std::string name, std::string login, std::string password)
        : m_name(std::move(name))
        , m_login(std::move(login))
        , m_password(std::move(password))
    {}

    inline const std::string& GetName() const {
        return m_name;
    }

    inline const std::string& GetLogin() const {
        return m_login;
    }

    inline bool CheckPassword(const std::string& pwd) const {
        return m_password == pwd;
    }

private:
    std::string m_name;
    std::string m_login;
    std::string m_password;
};

} // namespace console_chat
