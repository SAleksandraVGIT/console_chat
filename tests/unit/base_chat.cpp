#include "console_chat/core/base_chat.h"

#include <gtest/gtest.h>

#include <string>


using console_chat::core::BaseChat;
using console_chat::core::Message;
using console_chat::core::MAX_MESSAGES_PER_CHAT;

namespace {

TEST(BaseChat, Defaults) {
    BaseChat chat;

    EXPECT_TRUE(chat.IsParticipant("any_user"));
    EXPECT_FALSE(chat.IsPrivate());
    EXPECT_TRUE(chat.GetMessages().empty());
}

TEST(BaseChat, AddMessage) {
    BaseChat chat;

    EXPECT_TRUE(chat.AddMessage(Message{"User_1", "Hello"}));
    EXPECT_TRUE(chat.AddMessage(Message{"User_2", "Hi"}));

    const auto& messages = chat.GetMessages();
    ASSERT_EQ(messages.size(), 2u);
    EXPECT_EQ(messages[0].Name, "User_1");
    EXPECT_EQ(messages[0].Text, "Hello");
    EXPECT_EQ(messages[1].Name, "User_2");
    EXPECT_EQ(messages[1].Text, "Hi");
}

TEST(BaseChat, MessageLimit) {
    BaseChat chat;

    for (size_t i = 0; i < MAX_MESSAGES_PER_CHAT; ++i) {
        EXPECT_TRUE(chat.AddMessage(Message{"User_1", "Message " + std::to_string(i)}));
    }

    EXPECT_FALSE(chat.AddMessage(Message{"User_1", "Overflow"}));
    EXPECT_EQ(chat.GetMessages().size(), MAX_MESSAGES_PER_CHAT);
}

} // namespace
