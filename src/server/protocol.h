#pragma once

#include <string>
#include <vector>

namespace console_chat::server {

std::vector<std::string> Split(const std::string& line, char delim);
std::string Join(const std::vector<std::string>& parts, char delim);

} // namespace console_chat::server
