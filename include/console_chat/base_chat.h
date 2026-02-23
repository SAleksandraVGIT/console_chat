#pragma once

#include <string>
#include <vector>
#include <utility>


namespace console_chat {

struct Message {
    std::string Name;
    std::string Text;
};

class BaseChat {
public:
    explicit BaseChat(std::string name)
        : m_name(std::move(name)) {}

    virtual ~BaseChat() = default;

    virtual bool IsParticipant(const std::string&) const;
    virtual bool IsPrivate() const;

    inline const std::string& GetName() const {
        return m_name;
    }

    inline const std::vector<Message>& GetMessages() const {
        return m_messages;
    }

    bool AddMessage(Message&& msg);

protected:
    std::string m_name;
    std::vector<Message> m_messages;
};

} // namespace console_chat
