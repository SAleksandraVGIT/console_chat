#pragma once

#include <string>
#include <vector>
#include <utility>


namespace console_chat::core {

inline constexpr  size_t MAX_MESSAGES_PER_CHAT = 1000;

struct Message {
    std::string Name;
    std::string Text;
};

class BaseChat {
public:
    virtual ~BaseChat() = default;

    virtual bool IsParticipant(const std::string&) const;
    virtual bool IsPrivate() const;

    inline const std::vector<Message>& GetMessages() const {
        return m_messages;
    }

    bool AddMessage(Message&& msg);

protected:
    std::vector<Message> m_messages;
};

} // namespace console_chat::core
