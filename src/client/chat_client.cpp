#include "console_chat/client/chat_client.h"

#include <stdexcept>
#include <string>
#include <utility>

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

std::vector<std::string> ChatClient::Request(const std::vector<std::string>& parts, const bool background) const {
    const std::string payload = (background ? "POLL\t" : "") + Join(parts, '\t');
    if (!m_socket.SendLine(payload)) {
        throw std::runtime_error("Failed to send request.");
    }

    std::string line;
    if (!m_socket.RecvLine(line)) {
        throw std::runtime_error("Failed to receive response.");
    }

    auto response = Split(line, '\t');
    if (response.size() >= 2 &&
        response[0] == "ERR") {
        if (response[1] == "disconnected by ADMIN") {
            throw std::runtime_error("Disconnected by ADMIN.");
        }
        if (response[1] == "disconnected by inactivity timeout") {
            throw std::runtime_error("Disconnected due to inactivity timeout.");
        }
    }

    if (background && (response.empty() || response[0] != "OK")) {
        throw std::runtime_error("Auto-refresh failed: " +
            (response.size() > 1 ? response[1] : "invalid response"));
    }
    return response;
}

void ChatClient::NotifyActivity() const {
    const auto response = Request({"ACTIVITY"});
    if (response.empty() || response[0] != "OK") {
        throw std::runtime_error("Failed to report user activity.");
    }
}

bool ChatClient::Register(std::string&& name, std::string&& login, std::string&& password) {
    const auto resp = Request({"REGISTER", name, login, password});
    return !resp.empty() && resp[0] == "OK";
}

bool ChatClient::Authenticate(const std::string& login, const std::string& password) {
    return AuthenticateDetailed(login, password).Success;
}

AuthResult ChatClient::AuthenticateDetailed(const std::string& login, const std::string& password) {
    const auto resp = Request({"LOGIN", login, password});
    AuthResult result;
    result.Success = !resp.empty() && resp[0] == "OK";

    if (!result.Success && resp.size() >= 2) {
        result.Error = resp[1];
    }

    if (!result.Success && resp.size() >= 5 && resp[1] == "banned") {
        result.BanStatus = resp[2];
        result.BannedUntilEpoch = resp[3];
        result.ServerNowEpoch = resp[4];
    }

    return result;
}

void ChatClient::Logout() {
    Request({"LOGOUT"});
}

bool ChatClient::DeleteAccount(const std::string& confirmationLogin) {
    const auto resp = Request({"DELETE_ACCOUNT", confirmationLogin});
    return !resp.empty() && resp[0] == "OK";
}

bool ChatClient::IsAuthenticated() const {
    const auto resp = Request({"IS_AUTH"});
    return resp.size() >= 2 && resp[0] == "OK" && resp[1] == "1";
}

std::string ChatClient::GetCurrentUserLogin() const {
    const auto resp = Request({"CUR_LOGIN"});
    return (resp.size() >= 2 && resp[0] == "OK") ? resp[1] : std::string{};
}

std::string ChatClient::GetCurrentUserName() const {
    const auto resp = Request({"CUR_USER"});
    return (resp.size() >= 2 && resp[0] == "OK") ? resp[1] : std::string{};
}

std::vector<std::string> ChatClient::GetMyChats(const bool background) const {
    const auto resp = Request({"GET_MY_CHATS"}, background);
    if (resp.empty() || resp[0] != "OK") {
        return {};
    }
    return std::vector<std::string>(resp.begin() + 1, resp.end());
}

bool ChatClient::CreatePrivateChat(std::string&& recipientLogin, std::string&& chatName) {
    return CreatePrivateChatDetailed(
        std::move(recipientLogin),
        std::move(chatName)).Success;
}

CreatePrivateChatResult ChatClient::CreatePrivateChatDetailed(
    std::string&& recipientLogin,
    std::string&& chatName)
{
    const auto resp = Request({"CREATE_PRIVATE", recipientLogin, chatName});
    if (!resp.empty() && resp[0] == "OK") {
        return {true, {}};
    }

    if (resp.size() >= 3 &&
        resp[0] == "ERR" &&
        resp[1] == "chat already exists")
    {
        return {false, resp[2]};
    }

    return {};
}

std::vector<core::Message> ChatClient::GetMessages(const std::string& chatName, const bool background) const {
    const auto resp = Request({"GET_MESSAGES", chatName}, background);
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

std::vector<std::string> ChatClient::GetAllUserLogins(const bool background) const {
    const auto resp = Request({"GET_ALL_USERS"}, background);
    if (resp.empty() || resp[0] != "OK") {
        return {};
    }
    return std::vector<std::string>(resp.begin() + 1, resp.end());
}

bool ChatClient::AdminLogin(const std::string& login, const std::string& password) {
    const auto resp = Request({"ADMIN_LOGIN", login, password});
    return !resp.empty() && resp[0] == "OK";
}

std::vector<AdminUserInfo> ChatClient::AdminGetUsers(const bool background) const {
    const auto resp = Request({"ADMIN_GET_USERS"}, background);
    std::vector<AdminUserInfo> result;
    if (resp.empty() || resp[0] != "OK") {
        return result;
    }

    for (size_t i = 1; i + 4 < resp.size(); i += 5) {
        result.push_back({resp[i], resp[i + 1], resp[i + 2], resp[i + 3], resp[i + 4]});
    }

    return result;
}

std::vector<AdminChatInfo> ChatClient::AdminGetChats(const bool background) const {
    const auto resp = Request({"ADMIN_GET_CHATS"}, background);
    if (resp.empty() || resp[0] != "OK") {
        return {};
    }

    std::vector<AdminChatInfo> result;
    for (size_t i = 1; i + 3 < resp.size(); i += 4) {
        result.push_back({resp[i], resp[i + 1], resp[i + 2], resp[i + 3]});
    }

    return result;
}

bool ChatClient::AdminCreatePrivateChat(std::string&& recipientLogin, std::string&& chatName) {
    return AdminCreatePrivateChatDetailed(
        std::move(recipientLogin),
        std::move(chatName)).Success;
}

CreatePrivateChatResult ChatClient::AdminCreatePrivateChatDetailed(
    std::string&& recipientLogin,
    std::string&& chatName)
{
    const auto resp = Request({"ADMIN_CREATE_PRIVATE", recipientLogin, chatName});
    if (!resp.empty() && resp[0] == "OK") {
        return {true, {}};
    }

    if (resp.size() >= 3 &&
        resp[0] == "ERR" &&
        resp[1] == "chat already exists")
    {
        return {false, resp[2]};
    }

    return {};
}

std::vector<core::Message> ChatClient::AdminGetMessages(const std::string& chatName, const bool background) const {
    const auto resp = Request({"ADMIN_GET_MESSAGES", chatName}, background);
    std::vector<core::Message> result;
    if (resp.empty() || resp[0] != "OK") {
        return result;
    }

    for (size_t i = 1; i + 1 < resp.size(); i += 2) {
        result.push_back({resp[i], resp[i + 1]});
    }

    return result;
}

bool ChatClient::AdminSendMessageToChat(const std::string& chatName, std::string&& text) {
    const auto resp = Request({"ADMIN_SEND_CHAT", chatName, text});
    return !resp.empty() && resp[0] == "OK";
}

bool ChatClient::AdminSendGeneral(std::string&& text) {
    const auto resp = Request({"ADMIN_SEND_GENERAL", text});
    return !resp.empty() && resp[0] == "OK";
}

bool ChatClient::AdminKickUser(const std::string& login) {
    const auto resp = Request({"ADMIN_KICK_USER", login});
    return !resp.empty() && resp[0] == "OK";
}

bool ChatClient::AdminBanUser(const std::string& login, const std::string& period) {
    const auto resp = Request({"ADMIN_BAN_USER", login, period});
    return !resp.empty() && resp[0] == "OK";
}

bool ChatClient::AdminUnbanUser(const std::string& login) {
    const auto resp = Request({"ADMIN_UNBAN_USER", login});
    return !resp.empty() && resp[0] == "OK";
}

int ChatClient::AdminDeleteForeverBannedUsers() {
    const auto resp = Request({"ADMIN_DELETE_FOREVER_BANNED_USERS"});
    if (resp.size() < 2 || resp[0] != "OK") {
        return -1;
    }

    try {
        return std::stoi(resp[1]);
    } catch (const std::exception&) {
        return -1;
    }
}

} // namespace console_chat::client
