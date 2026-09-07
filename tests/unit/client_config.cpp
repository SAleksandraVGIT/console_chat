#include "console_chat/client/client_config.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

class ClientConfigTest : public ::testing::Test {
protected:
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("console_chat_client_config_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));

    void TearDown() override {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};

TEST_F(ClientConfigTest, MissingFileUsesThreeSeconds) {
    console_chat::client::ClientConfig config{std::chrono::milliseconds{100}};
    std::string error;
    ASSERT_TRUE(console_chat::client::LoadClientConfig(path.string(), config, error));
    EXPECT_EQ(config.RefreshInterval, std::chrono::milliseconds{3000});
    EXPECT_FALSE(error.empty());
}

TEST_F(ClientConfigTest, LoadsWhitespaceCommentsAndCrLf) {
    { std::ofstream file(path); file << "# interval\r\n\r\n refresh_interval_ms = 750 \r\n"; }
    console_chat::client::ClientConfig config;
    std::string error;
    ASSERT_TRUE(console_chat::client::LoadClientConfig(path.string(), config, error)) << error;
    EXPECT_EQ(config.RefreshInterval, std::chrono::milliseconds{750});
    EXPECT_TRUE(error.empty());
}

TEST_F(ClientConfigTest, RejectsInvalidIntervalsAndDuplicateOrUnknownKeys) {
    for (const auto& value : {"0", "-1", "99", "60001", "999999999999999999999999",
                              "", "100ms", "3.5", "100\nrefresh_interval_ms=200", "100\nunknown=5"})
    {
        SCOPED_TRACE(value);
        { std::ofstream file(path); file << "refresh_interval_ms=" << value << "\n"; }
        console_chat::client::ClientConfig config;
        std::string error;
        EXPECT_FALSE(console_chat::client::LoadClientConfig(path.string(), config, error));
        EXPECT_FALSE(error.empty());
        EXPECT_EQ(config.RefreshInterval, std::chrono::milliseconds{3000});
    }
}

} // namespace
