#include "console_chat/logging/logger.h"

#include <filesystem>
#include <stdexcept>
#include <utility>


namespace fs = std::filesystem;

namespace console_chat::logging {

namespace {

void EnsureParentDirectory(const std::string& filePath) {
    const auto parent = fs::path(filePath).parent_path();
    if (parent.empty()) {
        return;
    }

    std::error_code ec;
    fs::create_directories(parent, ec);
    if (ec) {
        throw std::runtime_error("Failed to create log directory: " + parent.string());
    }
}

} // namespace

Logger::Logger(std::string filePath)
    : m_filePath(std::move(filePath))
{
    EnsureParentDirectory(m_filePath);

    m_output.open(m_filePath, std::ios::app);
    if (!m_output) {
        throw std::runtime_error("Failed to open log file for writing: " + m_filePath);
    }

    m_input.open(m_filePath);
    if (!m_input) {
        throw std::runtime_error("Failed to open log file for reading: " + m_filePath);
    }
}

Logger::~Logger() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_output.is_open()) {
        m_output.close();
    }
    if (m_input.is_open()) {
        m_input.close();
    }
}

bool Logger::WriteLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_output << line << '\n';
    m_output.flush();
    return static_cast<bool>(m_output);
}

bool Logger::ReadLine(std::string& line) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_input.clear();
    return static_cast<bool>(std::getline(m_input, line));
}

} // namespace console_chat::logging
