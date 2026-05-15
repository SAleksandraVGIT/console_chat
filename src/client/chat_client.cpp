#include "console_chat/client/chat_client.h"

#include <stdexcept>
#include <string>

namespace console_chat::client {

namespace {

std::string Join(const std::vector<std::string>& parts, char delim) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (parts[i].find(delim) != std::string::npos || parts[i].find('\n') != std::string::npos) {
            throw std::runtime_error("Unsupported character in payload.");
        }

        out += parts[i];
        if (i + 1 < parts.size()) {
            out += delim;
        }
    }
    out += '\n';
    return out;
}

} // namespace

ChatClient::ChatClient(const std::string& host, const int port) {
    m_socket.Connect(host, static_cast<uint16_t>(port));
}

ChatClient::~ChatClient() = default;

std::vector<std::string> ChatClient::Split(const std::string& line, const char delim) {
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

std::vector<std::string> ChatClient::Request(const std::vector<std::string>& parts) const {
    const std::string payload = Join(parts, '\t');
    if (!m_socket.SendLine(payload)) {
        throw std::runtime_error("Failed to send request.");
    }

    std::string line;
    if (!m_socket.RecvLine(line)) {
        throw std::runtime_error("Failed to receive response.");
    }

    return Split(line, '\t');
}

bool ChatClient::Register(std::string&& name, std::string&& login, std::string&& password) {
    const auto resp = Request({"REGISTER", name, login, password});
    return !resp.empty() && resp[0] == "OK";
}

bool ChatClient::Authenticate(const std::string& login, const std::string& password) {
    const auto resp = Request({"LOGIN", login, password});
    return !resp.empty() && resp[0] == "OK";
}

void ChatClient::Logout() {
    Request({"LOGOUT"});
}

bool ChatClient::IsAuthenticated() const {
    const auto resp = Request({"IS_AUTH"});
    return resp.size() >= 2 && resp[0] == "OK" && resp[1] == "1";
}

std::string ChatClient::GetCurrentUserName() const {
    const auto resp = Request({"CUR_USER"});
    return (resp.size() >= 2 && resp[0] == "OK") ? resp[1] : std::string{};
}

std::vector<std::string> ChatClient::GetMyChats() const {
    const auto resp = Request({"GET_MY_CHATS"});
    if (resp.empty() || resp[0] != "OK") {
        return {};
    }
    return std::vector<std::string>(resp.begin() + 1, resp.end());
}

bool ChatClient::CreatePrivateChat(std::string&& recipientLogin, std::string&& chatName) {
    const auto resp = Request({"CREATE_PRIVATE", recipientLogin, chatName});
    return !resp.empty() && resp[0] == "OK";
}

std::vector<core::Message> ChatClient::GetMessages(const std::string& chatName) const {
    const auto resp = Request({"GET_MESSAGES", chatName});
    std::vector<core::Message> result;
    if (resp.empty() || resp[0] != "OK") {
        return result;
    }

    for (size_t i = 1; i + 1 < resp.size(); i += 2) {
        result.push_back({resp[i], resp[i + 1]});
    }

    return result;
}

bool ChatClient::SendMessage(const std::string& chatName, std::string&& text) {
    const auto resp = Request({"SEND_MESSAGE", chatName, text});
    return !resp.empty() && resp[0] == "OK";
}

std::vector<std::string> ChatClient::GetAllUserLogins() const {
    const auto resp = Request({"GET_ALL_USERS"});
    if (resp.empty() || resp[0] != "OK") {
        return {};
    }
    return std::vector<std::string>(resp.begin() + 1, resp.end());
}

} // namespace console_chat::client
