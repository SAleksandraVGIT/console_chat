#pragma once

#include <fstream>
#include <mutex>
#include <string>


namespace console_chat::logging {

class Logger {
public:
    explicit Logger(std::string filePath);
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    bool WriteLine(const std::string& line);
    bool ReadLine(std::string& line);

private:
    std::string m_filePath;
    std::ofstream m_output;
    std::ifstream m_input;
    std::mutex m_mutex;
};

} // namespace console_chat::logging
