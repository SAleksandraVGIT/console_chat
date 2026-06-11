#include "console_chat/core/private_chat.h"

#include <gtest/gtest.h>


using console_chat::core::PrivateChat;

namespace {

TEST(PrivateChat, Users) {
    PrivateChat chat("user_1", "user_2");

    EXPECT_TRUE(chat.HasUser("user_1"));
    EXPECT_TRUE(chat.HasUser("user_2"));
    EXPECT_FALSE(chat.HasUser("user_3"));
}

TEST(PrivateChat, Access) {
    PrivateChat chat("user_1", "user_2");

    EXPECT_TRUE(chat.IsPrivate());
    EXPECT_TRUE(chat.IsParticipant("user_1"));
    EXPECT_TRUE(chat.IsParticipant("user_2"));
    EXPECT_FALSE(chat.IsParticipant("user_3"));
}

TEST(PrivateChat, UserOrder) {
    PrivateChat chat("user_1", "user_2");

    const auto& users = chat.GetUsers();
    EXPECT_EQ(users[0], "user_1");
    EXPECT_EQ(users[1], "user_2");
}

} // namespace
