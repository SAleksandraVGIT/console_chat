#include "console_chat/private_chat.h"


namespace console_chat {

bool PrivateChat::IsParticipant(const std::string& login) const {
    return std::find(m_users.begin(), m_users.end(), login) != m_users.end();
}

bool PrivateChat::IsPrivate() const {
    return true;
}

} // namespace console_chat
