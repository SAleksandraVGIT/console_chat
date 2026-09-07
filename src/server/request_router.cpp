#include "request_router.h"

#include <chrono>
#include <cstdint>

namespace console_chat::server {

namespace {

bool ParseBanPeriod(const std::string& value, core::BanPeriod& period) {
    if (value == "1d") {
        period = core::BanPeriod::OneDay;
        return true;
    }
    if (value == "10d") {
        period = core::BanPeriod::TenDays;
        return true;
    }
    if (value == "1m") {
        period = core::BanPeriod::Month;
        return true;
    }
    if (value == "1y") {
        period = core::BanPeriod::Year;
        return true;
    }
    if (value == "forever") {
        period = core::BanPeriod::Forever;
        return true;
    }
    return false;
}

bool RequireAdmin(const RequestContext& context) {
    return context.isAdmin;
}

std::int64_t CurrentEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace

std::vector<std::string> HandleRequest(
    const std::vector<std::string>& req,
    core::ChatService& service,
    RequestContext& context,
    const AdminCredentials& adminCredentials,
    SessionController* sessionController)
{
    if (req.empty()) {
        return {"ERR", "empty request"};
    }

    const std::string& cmd = req[0];

    if (cmd == "POLL") {
        if (context.currentLogin.empty() && !context.isAdmin) {
            return {"ERR", "access denied"};
        }
        if (req.size() < 2) {
            return {"ERR", "invalid poll request"};
        }
        const auto& query = req[1];
        const bool list = query == "GET_MY_CHATS" || query == "GET_ALL_USERS" ||
            query == "ADMIN_GET_USERS" || query == "ADMIN_GET_CHATS";
        const bool messages = query == "GET_MESSAGES" || query == "ADMIN_GET_MESSAGES";
        if ((list && req.size() == 2) || (messages && req.size() == 3)) {
            return HandleRequest(std::vector<std::string>(req.begin() + 1, req.end()),
                service, context, adminCredentials, sessionController);
        }
        return {"ERR", "invalid poll request"};
    }

    if (cmd == "ACTIVITY" && req.size() == 1) {
        return (context.currentLogin.empty() && !context.isAdmin)
            ? std::vector<std::string>{"ERR", "access denied"}
            : std::vector<std::string>{"OK"};
    }

    if (cmd == "ADMIN_LOGIN" && req.size() == 3) {
        if (!adminCredentials.Enabled ||
            req[1] != adminCredentials.Login ||
            req[2] != adminCredentials.Password)
        {
            return {"ERR", "admin auth failed"};
        }

        context.currentLogin.clear();
        context.isAdmin = true;
        return {"OK"};
    }

    if (cmd == "REGISTER" && req.size() == 4) {
        if (context.isAdmin) {
            return {"ERR", "access denied"};
        }
        const bool ok = service.Register(std::string(req[1]), std::string(req[2]), std::string(req[3]));
        return ok ? std::vector<std::string>{"OK"} : std::vector<std::string>{"ERR", "register failed"};
    }

    if (cmd == "LOGIN" && req.size() == 3) {
        if (context.isAdmin) {
            return {"ERR", "access denied"};
        }

        core::UserInfo userInfo;
        if (service.GetUserInfo(req[1], userInfo) && userInfo.BannedNow) {
            return {
                "ERR",
                "banned",
                userInfo.BannedForever ? "FOREVER" : "TEMP",
                std::to_string(userInfo.BannedUntilEpoch),
                std::to_string(CurrentEpochSeconds())};
        }

        if (!service.Authenticate(req[1], req[2])) {
            return {"ERR", "auth failed"};
        }
        context.currentLogin = req[1];
        return {"OK"};
    }

    if (cmd == "LOGOUT" && req.size() == 1) {
        context.currentLogin.clear();
        context.isAdmin = false;
        return {"OK"};
    }

    if (cmd == "DELETE_ACCOUNT" && req.size() == 2) {
        if (context.isAdmin || context.currentLogin.empty()) {
            return {"ERR", "access denied"};
        }

        if (req[1] != context.currentLogin) {
            return {"ERR", "confirmation mismatch"};
        }

        const bool ok = service.DeleteUserAccount(context.currentLogin);
        if (ok) {
            context.currentLogin.clear();
        }
        return ok
            ? std::vector<std::string>{"OK"}
            : std::vector<std::string>{"ERR", "delete account failed"};
    }

    if (cmd == "IS_AUTH" && req.size() == 1) {
        return {"OK", (context.currentLogin.empty() && !context.isAdmin) ? "0" : "1"};
    }

    if (cmd == "CUR_USER" && req.size() == 1) {
        return {"OK", context.isAdmin ? core::ADMIN_MESSAGE_NAME : service.GetUserNameByLogin(context.currentLogin)};
    }

    if (cmd == "CUR_LOGIN" && req.size() == 1) {
        return {"OK", context.isAdmin ? core::ADMIN_MESSAGE_NAME : context.currentLogin};
    }

    if (cmd == "GET_MY_CHATS" && req.size() == 1) {
        auto chats = service.GetMyChats(context.currentLogin);
        std::vector<std::string> resp{"OK"};
        resp.insert(resp.end(), chats.begin(), chats.end());
        return resp;
    }

    if (cmd == "CREATE_PRIVATE" && req.size() == 3) {
        const auto existingChatName = service.GetPrivateChatName(context.currentLogin, req[1]);
        if (!existingChatName.empty()) {
            return {"ERR", "chat already exists", existingChatName};
        }

        const bool ok = service.CreatePrivateChat(context.currentLogin, std::string(req[1]), std::string(req[2]));
        return ok ? std::vector<std::string>{"OK"} : std::vector<std::string>{"ERR", "create private failed"};
    }

    if (cmd == "GET_MESSAGES" && req.size() == 2) {
        const auto messages = service.GetMessages(context.currentLogin, req[1]);
        std::vector<std::string> resp{"OK"};
        for (const auto& msg : messages) {
            resp.push_back(msg.Name);
            resp.push_back(msg.Text);
        }
        return resp;
    }

    if (cmd == "SEND_MESSAGE" && req.size() == 3) {
        const bool ok = service.SendMessage(context.currentLogin, req[1], std::string(req[2]));
        return ok ? std::vector<std::string>{"OK"} : std::vector<std::string>{"ERR", "send failed"};
    }

    if (cmd == "GET_ALL_USERS" && req.size() == 1) {
        auto users = service.GetAllUserLogins();
        std::vector<std::string> resp{"OK"};
        resp.insert(resp.end(), users.begin(), users.end());
        return resp;
    }

    if (cmd == "ADMIN_GET_USERS" && req.size() == 1) {
        if (!RequireAdmin(context)) {
            return {"ERR", "access denied"};
        }

        const auto users = service.GetAllUsersInfo();
        const auto serverNow = std::to_string(CurrentEpochSeconds());
        std::vector<std::string> resp{"OK"};
        for (const auto& user : users) {
            resp.push_back(user.Login);
            resp.push_back(user.Name);
            resp.push_back(user.BannedForever ? "FOREVER" : (user.BannedNow ? "TEMP" : "NONE"));
            resp.push_back(std::to_string(user.BannedUntilEpoch));
            resp.push_back(serverNow);
        }
        return resp;
    }

    if (cmd == "ADMIN_GET_CHATS" && req.size() == 1) {
        if (!RequireAdmin(context)) {
            return {"ERR", "access denied"};
        }

        const auto chats = service.GetAllChatsInfo();
        std::vector<std::string> resp{"OK"};
        for (const auto& chat : chats) {
            resp.push_back(chat.Name);
            resp.push_back(chat.IsPrivate ? "PRIVATE" : "GENERAL");
            resp.push_back(chat.Participants.size() > 0 ? chat.Participants[0] : std::string{});
            resp.push_back(chat.Participants.size() > 1 ? chat.Participants[1] : std::string{});
        }
        return resp;
    }

    if (cmd == "ADMIN_CREATE_PRIVATE" && req.size() == 3) {
        if (!RequireAdmin(context)) {
            return {"ERR", "access denied"};
        }

        const auto existingChatName =
            service.GetPrivateChatName(core::ADMIN_SYSTEM_LOGIN, req[1]);
        if (!existingChatName.empty()) {
            return {"ERR", "chat already exists", existingChatName};
        }

        const bool ok = service.CreateAdminPrivateChat(std::string(req[1]), std::string(req[2]));
        return ok
            ? std::vector<std::string>{"OK"}
            : std::vector<std::string>{"ERR", "create private failed"};
    }

    if (cmd == "ADMIN_GET_MESSAGES" && req.size() == 2) {
        if (!RequireAdmin(context)) {
            return {"ERR", "access denied"};
        }

        const auto messages = service.GetMessagesForAdmin(req[1]);
        std::vector<std::string> resp{"OK"};
        for (const auto& msg : messages) {
            resp.push_back(msg.Name);
            resp.push_back(msg.Text);
        }
        return resp;
    }

    if (cmd == "ADMIN_SEND_CHAT" && req.size() == 3) {
        if (!RequireAdmin(context)) {
            return {"ERR", "access denied"};
        }

        const bool ok = service.SendAdminMessageToChat(req[1], std::string(req[2]));
        return ok ? std::vector<std::string>{"OK"} : std::vector<std::string>{"ERR", "send failed"};
    }

    if (cmd == "ADMIN_SEND_GENERAL" && req.size() == 2) {
        if (!RequireAdmin(context)) {
            return {"ERR", "access denied"};
        }

        const bool ok = service.SendAdminMessageToGeneral(std::string(req[1]));
        return ok ? std::vector<std::string>{"OK"} : std::vector<std::string>{"ERR", "send failed"};
    }

    if (cmd == "ADMIN_KICK_USER" && req.size() == 2) {
        if (!RequireAdmin(context)) {
            return {"ERR", "access denied"};
        }

        const bool ok = sessionController && sessionController->KickUser(req[1]);
        return ok ? std::vector<std::string>{"OK"} : std::vector<std::string>{"ERR", "kick failed"};
    }

    if (cmd == "ADMIN_BAN_USER" && req.size() == 3) {
        if (!RequireAdmin(context)) {
            return {"ERR", "access denied"};
        }

        core::BanPeriod period = core::BanPeriod::OneDay;
        if (!ParseBanPeriod(req[2], period)) {
            return {"ERR", "bad ban period"};
        }

        const bool ok = service.BanUser(req[1], period);
        if (ok && sessionController) {
            sessionController->KickUser(req[1]);
        }
        return ok ? std::vector<std::string>{"OK"} : std::vector<std::string>{"ERR", "ban failed"};
    }

    if (cmd == "ADMIN_UNBAN_USER" && req.size() == 2) {
        if (!RequireAdmin(context)) {
            return {"ERR", "access denied"};
        }

        const bool ok = service.UnbanUser(req[1]);
        return ok ? std::vector<std::string>{"OK"} : std::vector<std::string>{"ERR", "unban failed"};
    }

    if (cmd == "ADMIN_DELETE_FOREVER_BANNED_USERS" && req.size() == 1) {
        if (!RequireAdmin(context)) {
            return {"ERR", "access denied"};
        }

        size_t deletedCount = 0;
        const bool ok = service.DeleteForeverBannedUsers(deletedCount);
        return ok
            ? std::vector<std::string>{"OK", std::to_string(deletedCount)}
            : std::vector<std::string>{"ERR", "delete forever banned users failed"};
    }

    return {"ERR", "bad request"};
}

std::vector<std::string> HandleRequest(
    const std::vector<std::string>& req,
    core::ChatService& service,
    std::string& currentLogin)
{
    RequestContext context;
    context.currentLogin = currentLogin;
    const auto response = HandleRequest(req, service, context, AdminCredentials{});
    currentLogin = context.currentLogin;
    return response;
}

} // namespace console_chat::server
