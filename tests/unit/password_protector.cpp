#include "console_chat/core/password_protector.h"

#include <gtest/gtest.h>


using console_chat::core::PasswordProtector;

namespace {

TEST(PasswordProtector, Verify) {
    const auto hash = PasswordProtector::Hash("secret");

    EXPECT_TRUE(PasswordProtector::IsHash(hash));
    EXPECT_TRUE(PasswordProtector::Verify("secret", hash));
    EXPECT_FALSE(PasswordProtector::Verify("wrong", hash));
}

TEST(PasswordProtector, StableHash) {
    EXPECT_EQ(PasswordProtector::Hash("secret"), PasswordProtector::Hash("secret"));
}

TEST(PasswordProtector, DifferentHash) {
    EXPECT_NE(PasswordProtector::Hash("secret"), PasswordProtector::Hash("another"));
}

TEST(PasswordProtector, HashPrefix) {
    EXPECT_TRUE(PasswordProtector::IsHash(PasswordProtector::Hash("secret")));
    EXPECT_FALSE(PasswordProtector::IsHash("plain_text_password"));
    EXPECT_FALSE(PasswordProtector::IsHash(""));
}

} // namespace
