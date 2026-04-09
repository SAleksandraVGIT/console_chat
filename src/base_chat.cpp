#include "console_chat/base_chat.h"


namespace console_chat {

constexpr size_t  MAX_MESSAGES_PER_CHAT = 1000;

bool BaseChat::IsParticipant(const std::string&) const {
    return true;
}

bool BaseChat::IsPrivate() const {
    return false;
}

bool BaseChat::AddMessage(Message&& msg) {
    if (m_messages.size() >= MAX_MESSAGES_PER_CHAT) {
        return false;
    }

    m_messages.emplace_back(std::move(msg));
    return true;
}

} // namespace console_chat
