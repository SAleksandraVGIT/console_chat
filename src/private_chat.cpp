#include "console_chat/private_chat.h"

namespace console_chat {

bool PrivateChat::IsParticipant(const std::string& login) const {
    return login == m_user1 || login == m_user2;
}

bool PrivateChat::IsPrivate() const {
    return true;
}

} // namespace console_chat
