#include "request_router.h"

namespace console_chat::server {

std::vector<std::string> HandleRequest(
    const std::vector<std::string>& req,
    core::ChatService& service,
    std::string& currentLogin,
    const std::string& usersFilePath,
    const std::string& chatsFilePath)
{
    if (req.empty()) {
        return {"ERR", "empty request"};
    }

    const std::string& cmd = req[0];

    if (cmd == "REGISTER" && req.size() == 4) {
        const bool ok = service.Register(std::string(req[1]), std::string(req[2]), std::string(req[3]));
        if (ok && !service.SaveState(usersFilePath, chatsFilePath)) {
            return {"ERR", "state save failed"};
        }
        return ok ? std::vector<std::string>{"OK"} : std::vector<std::string>{"ERR", "register failed"};
    }

    if (cmd == "LOGIN" && req.size() == 3) {
        if (!service.Authenticate(req[1], req[2])) {
            return {"ERR", "auth failed"};
        }
        currentLogin = req[1];
        return {"OK"};
    }

    if (cmd == "LOGOUT" && req.size() == 1) {
        currentLogin.clear();
        return {"OK"};
    }

    if (cmd == "IS_AUTH" && req.size() == 1) {
        return {"OK", currentLogin.empty() ? "0" : "1"};
    }

    if (cmd == "CUR_USER" && req.size() == 1) {
        return {"OK", service.GetUserNameByLogin(currentLogin)};
    }

    if (cmd == "GET_MY_CHATS" && req.size() == 1) {
        auto chats = service.GetMyChats(currentLogin);
        std::vector<std::string> resp{"OK"};
        resp.insert(resp.end(), chats.begin(), chats.end());
        return resp;
    }

    if (cmd == "CREATE_PRIVATE" && req.size() == 3) {
        const bool ok = service.CreatePrivateChat(currentLogin, std::string(req[1]), std::string(req[2]));
        if (ok && !service.SaveState(usersFilePath, chatsFilePath)) {
            return {"ERR", "state save failed"};
        }
        return ok ? std::vector<std::string>{"OK"} : std::vector<std::string>{"ERR", "create private failed"};
    }

    if (cmd == "GET_MESSAGES" && req.size() == 2) {
        const auto messages = service.GetMessages(currentLogin, req[1]);
        std::vector<std::string> resp{"OK"};
        for (const auto& msg : messages) {
            resp.push_back(msg.Name);
            resp.push_back(msg.Text);
        }
        return resp;
    }

    if (cmd == "SEND_MESSAGE" && req.size() == 3) {
        const bool ok = service.SendMessage(currentLogin, req[1], std::string(req[2]));
        if (ok && !service.SaveState(usersFilePath, chatsFilePath)) {
            return {"ERR", "state save failed"};
        }
        return ok ? std::vector<std::string>{"OK"} : std::vector<std::string>{"ERR", "send failed"};
    }

    if (cmd == "GET_ALL_USERS" && req.size() == 1) {
        auto users = service.GetAllUserLogins();
        std::vector<std::string> resp{"OK"};
        resp.insert(resp.end(), users.begin(), users.end());
        return resp;
    }

    return {"ERR", "bad request"};
}

} // namespace console_chat::server
