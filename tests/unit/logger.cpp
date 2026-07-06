#include "console_chat/logging/logger.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>


namespace fs = std::filesystem;

namespace {

fs::path MakeTempPath(const std::string& fileName) {
    const auto uniquePart = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() /
        ("console_chat_logger_" + std::to_string(uniquePart) + "_" + fileName);
}

TEST(Logger, WritesAndReadsLines) {
    const auto logFile = MakeTempPath("log.txt");

    {
        console_chat::logging::Logger logger(logFile.string());

        EXPECT_TRUE(logger.WriteLine("first"));
        EXPECT_TRUE(logger.WriteLine("second"));

        std::string line;
        ASSERT_TRUE(logger.ReadLine(line));
        EXPECT_EQ(line, "first");
        ASSERT_TRUE(logger.ReadLine(line));
        EXPECT_EQ(line, "second");
        EXPECT_FALSE(logger.ReadLine(line));
    }

    std::error_code ec;
    fs::remove(logFile, ec);
}

TEST(Logger, AllowsConcurrentWrites) {
    const auto logFile = MakeTempPath("concurrent_log.txt");
    constexpr int THREAD_COUNT = 4;
    constexpr int MESSAGES_PER_THREAD = 25;

    {
        console_chat::logging::Logger logger(logFile.string());
        std::vector<std::thread> threads;

        for (int threadIndex = 0; threadIndex < THREAD_COUNT; ++threadIndex) {
            threads.emplace_back([&logger, threadIndex]() {
                for (int messageIndex = 0; messageIndex < MESSAGES_PER_THREAD; ++messageIndex) {
                    logger.WriteLine(
                        "thread=" + std::to_string(threadIndex) +
                        " message=" + std::to_string(messageIndex));
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        int lineCount = 0;
        std::string line;
        while (logger.ReadLine(line)) {
            ++lineCount;
        }

        EXPECT_EQ(lineCount, THREAD_COUNT * MESSAGES_PER_THREAD);
    }

    std::error_code ec;
    fs::remove(logFile, ec);
}

} // namespace
