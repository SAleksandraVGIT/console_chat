#include "console_chat/core/user.h"

#include "console_chat/core/password_protector.h"

#include <gtest/gtest.h>


using console_chat::core::PasswordProtector;
using console_chat::core::User;

namespace {

TEST(User, StoresData) {
    const auto hash = PasswordProtector::Hash("secret");
    const User user("User_1", hash);

    EXPECT_EQ(user.GetName(), "User_1");
    EXPECT_EQ(user.GetPasswordHash(), hash);
}

TEST(User, CheckPassword) {
    const User user("User_1", PasswordProtector::Hash("secret"));

    EXPECT_TRUE(user.CheckPassword("secret"));
    EXPECT_FALSE(user.CheckPassword("wrong"));
}

} // namespace
