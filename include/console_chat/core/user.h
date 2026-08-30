#pragma once

#include "password_protector.h"

#include <cstdint>
#include <string>
#include <utility>


namespace console_chat::core {

class User {
public:
    User(std::string name, std::string passwordHash,
         const std::int64_t bannedUntilEpoch = 0, const bool bannedForever = false)
        : m_name(std::move(name))
        , m_passwordHash(std::move(passwordHash))
        , m_bannedUntilEpoch(bannedUntilEpoch)
        , m_bannedForever(bannedForever)
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

    inline std::int64_t GetBannedUntilEpoch() const {
        return m_bannedUntilEpoch;
    }

    inline bool IsBannedForever() const {
        return m_bannedForever;
    }

    inline bool IsBannedAt(const std::int64_t nowEpoch) const {
        return m_bannedForever || (m_bannedUntilEpoch > nowEpoch);
    }

    inline void SetBan(const std::int64_t bannedUntilEpoch, const bool bannedForever) {
        m_bannedUntilEpoch = bannedUntilEpoch;
        m_bannedForever = bannedForever;
    }

private:
    std::string m_name;
    std::string m_passwordHash;
    std::int64_t m_bannedUntilEpoch = 0;
    bool m_bannedForever = false;
};

} // namespace console_chat::core
