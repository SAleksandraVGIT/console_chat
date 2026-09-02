#include "console_chat/client/chat_console.h"

#include "console_chat/core/chat_service.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <exception>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>


namespace console_chat::client {

constexpr size_t LAST_MESSAGE_COUNT = 15;
constexpr int INVALID_MENU_CHOICE = std::numeric_limits<int>::min();

std::string_view Trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }

    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }

    return value;
}

bool IsConnectionClosedError(const std::exception& error) {
    const std::string_view message = error.what();
    return message == "Disconnected by ADMIN." ||
        message == "Disconnected due to inactivity timeout." ||
        message == "Failed to send request." ||
        message == "Failed to receive response.";
}

bool ParseInt(const std::string& line, int& value) {
    const auto trimmed = Trim(line);
    if (trimmed.empty()) {
        return false;
    }

    const auto [ptr, ec] =
        std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), value);
    return ec == std::errc{} && ptr == trimmed.data() + trimmed.size();
}

bool AdminUserExists(const std::vector<AdminUserInfo>& users, const std::string& login) {
    return std::any_of(
        users.begin(),
        users.end(),
        [&login](const AdminUserInfo& user) {
            return user.Login == login;
        });
}

std::string ReadLine() {
    std::string input;
    std::getline(std::cin >> std::ws, input);
    return input;
}

int ReadInt() {
    std::string line;
    if (!std::getline(std::cin, line)) {
        return INVALID_MENU_CHOICE;
    }

    int value = 0;
    if (!ParseInt(line, value)) {
        return INVALID_MENU_CHOICE;
    }

    return value;
}

long long ParseLongLong(const std::string& value) {
    try {
        return std::stoll(value);
    } catch (const std::exception&) {
        return 0;
    }
}

std::string FormatDuration(std::chrono::seconds duration) {
    if (duration <= std::chrono::seconds::zero()) {
        return "less than a minute";
    }

    const auto days = std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24;
    duration -= std::chrono::hours(days * 24);
    const auto hours = std::chrono::duration_cast<std::chrono::hours>(duration).count();
    duration -= std::chrono::hours(hours);
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count();

    std::string result;
    if (days > 0) {
        result += std::to_string(days) + "d ";
    }
    if (hours > 0 || days > 0) {
        result += std::to_string(hours) + "h ";
    }
    result += std::to_string(minutes) + "m";
    return result;
}

std::string FormatBanRemaining(
    const std::string& bannedUntilEpoch,
    const std::string& serverNowEpoch)
{
    const auto until = ParseLongLong(bannedUntilEpoch);
    const auto now = ParseLongLong(serverNowEpoch);
    return FormatDuration(std::chrono::seconds(until - now));
}

std::string DisplayLogin(const std::string& login) {
    return login == core::ADMIN_SYSTEM_LOGIN ? core::ADMIN_MESSAGE_NAME : login;
}

bool IsAdminPrivateChat(const AdminChatInfo& chat) {
    return chat.FirstUserLogin == core::ADMIN_SYSTEM_LOGIN ||
        chat.SecondUserLogin == core::ADMIN_SYSTEM_LOGIN;
}

void PrintLastMessages(
    const std::vector<core::Message>& messages,
    const std::string& currentUserName,
    const size_t lastCount = LAST_MESSAGE_COUNT)
{
    const auto startIt =
        messages.size() > lastCount
            ? messages.end() - static_cast<std::ptrdiff_t>(lastCount)
            : messages.begin();

    std::for_each(startIt, messages.end(),
        [&](const core::Message& msg) {
            if (!currentUserName.empty() && msg.Name == currentUserName) {
                std::cout << "\tYou: " << msg.Text << "\n";
            } else {
                std::cout << "[" << msg.Name << "] " << msg.Text << "\n";
            }
        });
}

int ChatConsole::Run() {
    try {
        bool running = true;

        while (running) {
            ShowMainMenu();

            const int choice = ReadInt();

            switch (static_cast<ActionMainMenu>(choice)) {
                case ActionMainMenu::EXIT:
                    std::cout << "Session ended.\n";
                    running = false;
                    break;

                case ActionMainMenu::REGISTRATION:
                    RegistrationFlow();
                    break;

                case ActionMainMenu::LOGIN:
                    LoginFlow();
                    if (m_service.IsAuthenticated()) {
                        UserMenu();
                    }
                    break;

                default:
                    std::cout << "Invalid input.\n";
                    break;
            }
        }
    } catch (const std::exception& error) {
        if (!IsConnectionClosedError(error)) {
            throw;
        }
        std::cout << error.what() << "\n";
    }

    return 0;
}

int ChatConsole::RunAdmin() {
    try {
        bool running = true;

        while (running) {
            ShowAdminMainMenu();

            const int choice = ReadInt();

            switch (static_cast<ActionAdminMainMenu>(choice)) {
                case ActionAdminMainMenu::EXIT:
                    std::cout << "Admin session ended.\n";
                    running = false;
                    break;

                case ActionAdminMainMenu::LOGIN:
                    AdminLoginFlow();
                    if (m_service.IsAuthenticated()) {
                        AdminMenu();
                    }
                    break;

                default:
                    std::cout << "Invalid input.\n";
                    break;
            }
        }
    } catch (const std::exception& error) {
        if (!IsConnectionClosedError(error)) {
            throw;
        }
        std::cout << error.what() << "\n";
    }

    return 0;
}

void ChatConsole::UserMenu() {
    bool inSession = true;

    while (inSession && m_service.IsAuthenticated()) {
        ShowUserMenu();

        const int choice = ReadInt();

        switch (static_cast<ActionUserMenu>(choice)) {
            case ActionUserMenu::LOG_OUT:
                m_service.Logout();
                std::cout << "Logged out.\n";
                inSession = false;
                break;

            case ActionUserMenu::SHOW_MY_CHATS:
                ShowMyChatsFlow();
                break;

            case ActionUserMenu::CREATE_PRIVATE_CHAT:
                CreatePrivateChatFlow();
                break;

            case ActionUserMenu::OPEN_PRIVATE_CHAT:
                OpenChatFlow();
                break;

            case ActionUserMenu::OPEN_GENERAL_CHAT:
                OpenGeneralChatFlow();
                break;

            case ActionUserMenu::SHOW_ALL_USERS:
                ShowAllUsersFlow();
                break;

            case ActionUserMenu::DELETE_ACCOUNT:
                if (DeleteAccountFlow()) {
                    inSession = false;
                }
                break;

            default:
                std::cout << "Invalid input.\n";
                break;
        }
    }
}

void ChatConsole::AdminMenu() {
    bool inSession = true;

    while (inSession && m_service.IsAuthenticated()) {
        ShowAdminMenu();

        const int choice = ReadInt();

        switch (static_cast<ActionAdminMenu>(choice)) {
            case ActionAdminMenu::LOG_OUT:
                m_service.Logout();
                std::cout << "Logged out.\n";
                inSession = false;
                break;

            case ActionAdminMenu::SHOW_USERS:
                AdminShowUsersFlow();
                break;

            case ActionAdminMenu::SHOW_CHATS:
                AdminShowChatsFlow();
                break;

            case ActionAdminMenu::CREATE_PRIVATE_CHAT:
                AdminCreatePrivateChatFlow();
                break;

            case ActionAdminMenu::OPEN_PRIVATE_CHAT:
                AdminOpenPrivateChatFlow();
                break;

            case ActionAdminMenu::OPEN_GENERAL_CHAT:
                AdminOpenGeneralChatFlow();
                break;

            case ActionAdminMenu::KICK_USER:
                AdminKickUserFlow();
                break;

            case ActionAdminMenu::BAN_USER:
                AdminBanUserFlow();
                break;

            case ActionAdminMenu::UNBAN_USER:
                AdminUnbanUserFlow();
                break;

            case ActionAdminMenu::DELETE_FOREVER_BANNED_USERS:
                AdminDeleteForeverBannedUsersFlow();
                break;

            default:
                std::cout << "Invalid input.\n";
                break;
        }
    }
}

void ChatConsole::RegistrationFlow() {
    std::cout << "\n==== REGISTRATION ====\n"
              << "-Press \"/0\" to cancel-\n";

    std::cout << "Enter name: ";
    std::string name = ReadLine();
    if (name == "/0") {
        return;
    }

    std::cout << "Enter login: ";
    std::string login = ReadLine();
    if (login == "/0") {
        return;
    }

    std::cout << "Enter password: ";
    std::string password = ReadLine();
    if (password == "/0") {
        return;
    }

    const bool success = m_service.Register(std::move(name), std::move(login), std::move(password));

    std::cout << (success
                    ? "Registration successful.\n"
                    : "Registration failed. Login is unavailable.\n");
}

void ChatConsole::LoginFlow() {
    std::cout << "\n==== LOGIN ====\n"
              << "-Press \"/0\" to cancel-\n";

    std::cout << "Enter login: ";
    const std::string login = ReadLine();
    if (login == "/0") {
        return;
    }

    std::cout << "Enter password: ";
    const std::string password = ReadLine();
    if (password == "/0") {
        return;
    }

    const auto result = m_service.AuthenticateDetailed(login, password);

    if (result.Success) {
        std::cout << "Login successful.\n";
        return;
    }

    if (result.Error == "banned") {
        if (result.BanStatus == "FOREVER") {
            std::cout << "Your account is banned forever.\n";
        } else {
            std::cout << "Your account is banned. Time left: "
                      << FormatBanRemaining(result.BannedUntilEpoch, result.ServerNowEpoch)
                      << ".\n";
        }
        return;
    }

    std::cout << "Invalid login or password.\n";
}

void ChatConsole::AdminLoginFlow() {
    std::cout << "\n==== ADMIN LOGIN ====\n"
              << "-Press \"/0\" to cancel-\n";

    std::cout << "Enter admin login: ";
    const std::string login = ReadLine();
    if (login == "/0") {
        return;
    }

    std::cout << "Enter admin password: ";
    const std::string password = ReadLine();
    if (password == "/0") {
        return;
    }

    const bool success = m_service.AdminLogin(login, password);

    std::cout << (success
                    ? "Admin login successful.\n"
                    : "Invalid admin login or password.\n");
}

void ChatConsole::CreatePrivateChatFlow()
{
    std::cout << "\nEnter recipient login (\"/0\" to cancel): ";
    std::string login = ReadLine();

    if (login == "/0") {
        return;
    }

    std::cout << "Enter chat name: ";
    std::string chatName = ReadLine();

    if (chatName == "/0") {
        return;
    }

    const auto result = m_service.CreatePrivateChatDetailed(
        std::move(login),
        std::move(chatName));

    if (result.Success) {
        std::cout << "Private chat created.\n";
    } else if (!result.ExistingChatName.empty()) {
        std::cout << "Private chat already exists: "
                  << result.ExistingChatName << ".\n";
    } else {
        std::cout << "Failed to create private chat.\n";
    }
}

bool ChatConsole::DeleteAccountFlow() {
    const auto currentLogin = m_service.GetCurrentUserLogin();
    if (currentLogin.empty()) {
        std::cout << "Account deletion failed.\n";
        return false;
    }

    std::cout << "\n==== DELETE ACCOUNT ====\n"
              << "Enter your login to confirm account deletion (\"/0\" to cancel): ";

    const std::string confirmation = ReadLine();
    if (confirmation == "/0") {
        return false;
    }

    if (confirmation != currentLogin) {
        std::cout << "Confirmation does not match current login.\n";
        return false;
    }

    if (!m_service.DeleteAccount(confirmation)) {
        std::cout << "Account deletion failed.\n";
        return false;
    }

    std::cout << "Account deleted.\n";
    return true;
}

void ChatConsole::ChatSession(const std::string& chatName)
{
    std::cout << "\n==== CHAT: " << chatName << " ====\n";
    PrintLastMessages(m_service.GetMessages(chatName), m_service.GetCurrentUserName());

    std::cout << "Enter message (\"/0\" to exit, \"/all\" to print full chat):\n";

    while (true) {
        std::string text = ReadLine();

        if (text == "/0") {
            break;
        }

        if (text == "/all") {
            std::cout << "\n==== FULL CHAT ====\n";
            const auto allMessages = m_service.GetMessages(chatName);
            PrintLastMessages(allMessages, m_service.GetCurrentUserName(), allMessages.size());
            continue;
        }

        if (!m_service.SendMessage(chatName, std::move(text))) {
            std::cout << "Failed to send message.\n";
            break;
        }

        PrintLastMessages(m_service.GetMessages(chatName), m_service.GetCurrentUserName());
    }
}

void ChatConsole::OpenChatFlow()
{
    const auto chats = m_service.GetMyChats();
    if (chats.size() <= 1) {
        std::cout << "You have no private chats.\n";
        return;
    }

    std::cout << "\nEnter chat name (\"/0\" to cancel): ";
    const std::string chatName = ReadLine();

    if (chatName == "/0") {
        return;
    }

    const bool chatExists = std::any_of(chats.begin(), chats.end(),
        [&](const auto& chat) {
            return chat == chatName;
        });

    if (!chatExists) {
        std::cout << "Chat \"" << chatName << "\" does not exist.\n";
        return;
    }

    ChatSession(chatName);
}

void ChatConsole::OpenGeneralChatFlow() {
    ChatSession("GENERAL");
}

void ChatConsole::ShowMyChatsFlow() const {
    const auto chats = m_service.GetMyChats();

    if (chats.empty()) {
        std::cout << "You have no chats.\n";
        return;
    }

    std::cout << "\n==== MY CHATS ====\n";

    for (const auto& name : chats) {
        std::cout << "- " << name << "\n";
    }
}

void ChatConsole::ShowAllUsersFlow() const {
    const auto userLogins = m_service.GetAllUserLogins();
    const auto currentLogin = m_service.GetCurrentUserLogin();

    if (userLogins.empty()) {
        std::cout << "Users not found.\n";
        return;
    }

    std::cout << "\n==== ALL USER LOGINS ====\n";

    for (const auto& userLogin : userLogins) {
        std::cout << "- " << userLogin;
        if (!currentLogin.empty() && userLogin == currentLogin) {
            std::cout << " (You)";
        }
        std::cout << "\n";
    }
}

void ChatConsole::AdminShowUsersFlow() const {
    const auto users = m_service.AdminGetUsers();

    if (users.empty()) {
        std::cout << "Users not found.\n";
        return;
    }

    std::cout << "\n==== USERS ====\n";
    for (const auto& user : users) {
        std::cout << "- " << user.Login << " (" << user.Name << ")";
        if (user.BanStatus == "FOREVER") {
            std::cout << " [banned forever]";
        } else if (user.BanStatus == "TEMP") {
            std::cout << " [banned, time left: "
                      << FormatBanRemaining(user.BannedUntilEpoch, user.ServerNowEpoch)
                      << "]";
        }
        std::cout << "\n";
    }
}

void ChatConsole::AdminShowChatsFlow() const {
    const auto chats = m_service.AdminGetChats();

    if (chats.empty()) {
        std::cout << "Chats not found.\n";
        return;
    }

    std::cout << "\n==== CHATS ====\n";
    for (const auto& chat : chats) {
        std::cout << "- " << chat.Name << " [" << chat.Type << "]";
        if (chat.Type == "PRIVATE") {
            std::cout << " "
                      << DisplayLogin(chat.FirstUserLogin)
                      << " <-> "
                      << DisplayLogin(chat.SecondUserLogin);
        }
        std::cout << "\n";
    }
}

void ChatConsole::AdminCreatePrivateChatFlow() {
    std::cout << "\nEnter recipient login (\"/0\" to cancel): ";
    std::string login = ReadLine();
    if (login == "/0") {
        return;
    }

    std::cout << "Enter chat name: ";
    std::string chatName = ReadLine();
    if (chatName == "/0") {
        return;
    }

    const auto result = m_service.AdminCreatePrivateChatDetailed(
        std::move(login),
        std::move(chatName));
    if (result.Success) {
        std::cout << "Private chat created.\n";
    } else if (!result.ExistingChatName.empty()) {
        std::cout << "Private chat already exists: "
                  << result.ExistingChatName << ".\n";
    } else {
        std::cout << "Failed to create private chat.\n";
    }
}

void ChatConsole::AdminOpenPrivateChatFlow() {
    const auto allChats = m_service.AdminGetChats();
    std::vector<AdminChatInfo> chats;
    std::copy_if(
        allChats.begin(),
        allChats.end(),
        std::back_inserter(chats),
        [](const AdminChatInfo& chat) {
            return chat.Type == "PRIVATE";
        });

    if (chats.empty()) {
        std::cout << "Private chats not found.\n";
        return;
    }

    std::cout << "\n==== PRIVATE CHATS ====\n";
    for (const auto& chat : chats) {
        std::cout << "- " << chat.Name << " "
                  << DisplayLogin(chat.FirstUserLogin)
                  << " <-> "
                  << DisplayLogin(chat.SecondUserLogin)
                  << "\n";
    }

    std::cout << "\nEnter chat name (\"/0\" to cancel): ";
    const std::string chatName = ReadLine();
    if (chatName == "/0") {
        return;
    }

    const auto chat = std::find_if(
        chats.begin(),
        chats.end(),
        [&chatName](const AdminChatInfo& item) {
            return item.Name == chatName;
        });

    if (chat == chats.end()) {
        std::cout << "Chat \"" << chatName << "\" does not exist.\n";
        return;
    }

    AdminChatSession(chatName, IsAdminPrivateChat(*chat));
}

void ChatConsole::AdminOpenGeneralChatFlow() {
    AdminChatSession(core::GENERAL_CHAT_NAME, true);
}

void ChatConsole::AdminChatSession(const std::string& chatName, const bool canSend) {
    std::cout << "\n==== CHAT: " << chatName << " ====\n";
    PrintLastMessages(
        m_service.AdminGetMessages(chatName),
        core::ADMIN_MESSAGE_NAME,
        std::numeric_limits<size_t>::max());

    if (canSend) {
        std::cout << "Enter message (\"/0\" to exit, \"/all\" to print full chat):\n";
    } else {
        std::cout << "Read-only chat. Enter \"/0\" to exit or \"/all\" to print full chat:\n";
    }

    while (true) {
        std::string text = ReadLine();

        if (text == "/0") {
            break;
        }

        if (text == "/all") {
            std::cout << "\n==== FULL CHAT ====\n";
            const auto allMessages = m_service.AdminGetMessages(chatName);
            PrintLastMessages(allMessages, core::ADMIN_MESSAGE_NAME, allMessages.size());
            continue;
        }

        if (!canSend) {
            std::cout << "This chat is read-only for ADMIN.\n";
            continue;
        }

        const bool success = chatName == core::GENERAL_CHAT_NAME
            ? m_service.AdminSendGeneral(std::move(text))
            : m_service.AdminSendMessageToChat(chatName, std::move(text));

        if (!success) {
            std::cout << "Failed to send message.\n";
            break;
        }

        PrintLastMessages(
            m_service.AdminGetMessages(chatName),
            core::ADMIN_MESSAGE_NAME,
            std::numeric_limits<size_t>::max());
    }
}

void ChatConsole::AdminKickUserFlow() {
    std::cout << "\nEnter user login to kick (\"/0\" to cancel): ";
    const std::string login = ReadLine();
    if (login == "/0") {
        return;
    }

    const bool success = m_service.AdminKickUser(login);
    std::cout << (success ? "User disconnected.\n" : "Failed to disconnect user.\n");
}

void ChatConsole::AdminBanUserFlow() {
    std::cout << "\nEnter user login to ban (\"/0\" to cancel): ";
    const std::string login = ReadLine();
    if (login == "/0") {
        return;
    }

    const auto users = m_service.AdminGetUsers();
    if (!AdminUserExists(users, login)) {
        std::cout << "User \"" << login << "\" does not exist.\n";
        return;
    }

    std::cout << "Ban period:\n"
              << "1 - 1 day\n"
              << "2 - 10 days\n"
              << "3 - 1 month\n"
              << "4 - 1 year\n"
              << "5 - forever\n"
              << "-Press \"/0\" to cancel-\n";

    std::string choiceLine;
    std::getline(std::cin, choiceLine);
    if (Trim(choiceLine) == "/0") {
        return;
    }

    int choice = 0;
    if (!ParseInt(choiceLine, choice)) {
        std::cout << "Invalid input.\n";
        return;
    }

    std::string period;
    switch (choice) {
        case 1:
            period = "1d";
            break;
        case 2:
            period = "10d";
            break;
        case 3:
            period = "1m";
            break;
        case 4:
            period = "1y";
            break;
        case 5:
            period = "forever";
            break;
        default:
            std::cout << "Invalid input.\n";
            return;
    }

    const bool success = m_service.AdminBanUser(login, period);
    std::cout << (success ? "User banned.\n" : "Failed to ban user.\n");
}

void ChatConsole::AdminUnbanUserFlow() {
    std::cout << "\nEnter user login to unban (\"/0\" to cancel): ";
    const std::string login = ReadLine();
    if (login == "/0") {
        return;
    }

    const bool success = m_service.AdminUnbanUser(login);
    std::cout << (success ? "User unbanned.\n" : "Failed to unban user.\n");
}

void ChatConsole::AdminDeleteForeverBannedUsersFlow() {
    const auto users = m_service.AdminGetUsers();
    std::vector<AdminUserInfo> foreverBannedUsers;
    std::copy_if(
        users.begin(),
        users.end(),
        std::back_inserter(foreverBannedUsers),
        [](const AdminUserInfo& user) {
            return user.BanStatus == "FOREVER";
        });

    if (foreverBannedUsers.empty()) {
        std::cout << "Forever banned users not found.\n";
        return;
    }

    std::cout << "\n==== FOREVER BANNED USERS ====\n";
    for (const auto& user : foreverBannedUsers) {
        std::cout << "- " << user.Login << " (" << user.Name << ")\n";
    }

    std::cout << "Enter DELETE to remove these accounts (\"/0\" to cancel): ";
    const std::string confirmation = ReadLine();
    if (confirmation == "/0") {
        return;
    }

    if (confirmation != "DELETE") {
        std::cout << "Confirmation does not match DELETE.\n";
        return;
    }

    const int deletedCount = m_service.AdminDeleteForeverBannedUsers();
    if (deletedCount < 0) {
        std::cout << "Failed to delete forever banned users.\n";
        return;
    }

    std::cout << "Deleted accounts: " << deletedCount << ".\n";
}

void ChatConsole::ShowMainMenu() const {
    std::cout << "\n==== MAIN MENU ====\n"
              << "0 - Exit\n"
              << "1 - Registration\n"
              << "2 - Log in\n";
}

void ChatConsole::ShowAdminMainMenu() const {
    std::cout << "\n==== ADMIN MAIN MENU ====\n"
              << "0 - Exit\n"
              << "1 - Admin log in\n";
}

void ChatConsole::ShowUserMenu() const {
    std::cout << "\n==== USER MENU ====\n"
              << "0 - Log out\n"
              << "1 - Get list of my chats\n"
              << "2 - Create private chat\n"
              << "3 - Open private chat\n"
              << "4 - Open general chat\n"
              << "5 - Get list of user\n"
              << "6 - Delete account\n";
}

void ChatConsole::ShowAdminMenu() const {
    std::cout << "\n==== ADMIN MENU ====\n"
              << "0 - Log out\n"
              << "1 - Show users\n"
              << "2 - Show chats\n"
              << "3 - Create private chat\n"
              << "4 - Open private chat\n"
              << "5 - Open general chat\n"
              << "6 - Disconnect user\n"
              << "7 - Ban user\n"
              << "8 - Unban user\n"
              << "9 - Delete forever banned users\n";
}

} // namespace console_chat::client
