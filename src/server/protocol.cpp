#include "protocol.h"

namespace console_chat::server {

std::vector<std::string> Split(const std::string& line, const char delim) {
    std::vector<std::string> parts;
    std::string cur;
    for (char ch : line) {
        if (ch == delim) {
            parts.push_back(cur);
            cur.clear();
            continue;
        }
        cur.push_back(ch);
    }
    parts.push_back(cur);
    return parts;
}

std::string Join(const std::vector<std::string>& parts, const char delim) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        out += parts[i];
        if (i + 1 < parts.size()) {
            out.push_back(delim);
        }
    }
    out.push_back('\n');
    return out;
}

} // namespace console_chat::server
