#include "console_chat/core/base_chat.h"


namespace console_chat::core {

BaseChat::BaseChat(const size_t maxMessages)
    : m_maxMessages(maxMessages) {}

bool BaseChat::IsParticipant(const std::string&) const {
    return true;
}

bool BaseChat::IsPrivate() const {
    return false;
}

bool BaseChat::AddMessage(Message&& msg) {
    if (m_messages.size() >= m_maxMessages) {
        return false;
    }

    m_messages.emplace_back(std::move(msg));
    return true;
}

} // namespace console_chat::core
