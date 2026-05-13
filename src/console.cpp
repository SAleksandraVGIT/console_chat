#include "console_chat/console.h"

#include <iostream>
#include <utility>
#include <algorithm>


namespace console_chat {

constexpr size_t LAST_MESSAGE_COUNT = 15;

std::string ReadLine() {
    std::string input;
    std::getline(std::cin >> std::ws, input);
    return input;
}

int ReadInt() {
    std::string line;
    std::getline(std::cin, line);
    return std::stoi(line);
}

void PrintLastMessages(
    const std::vector<Message>& messages,
    const std::string& currentUserName,
    const size_t lastCount = LAST_MESSAGE_COUNT)
{
    const auto startIt =
        messages.size() > lastCount
            ? messages.end() - static_cast<std::ptrdiff_t>(lastCount)
            : messages.begin();

    std::for_each(startIt, messages.end(),
        [&](const Message& msg) {
            if (!currentUserName.empty() && msg.Name == currentUserName) {
                std::cout << "\tYou: " << msg.Text << "\n";
            } else {
                std::cout << "[" << msg.Name << "] " << msg.Text << "\n";
            }
        });
}

int Console::Run() {
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

    return 0;
}

void Console::UserMenu() {
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

            default:
                std::cout << "Invalid input.\n";
                break;
        }
    }
}

void Console::RegistrationFlow() {
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
                    : "Registration failed. Login is already exist.\n");
}

void Console::LoginFlow() {
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

    const bool success = m_service.Authenticate(login, password);

    std::cout << (success
                    ? "Login successful.\n"
                    : "Invalid login or password.\n");
}

void Console::CreatePrivateChatFlow()
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

    const bool success = m_service.CreatePrivateChat(std::move(login), std::move(chatName));

    std::cout << (success
                ? "Private chat created.\n"
                : "Failed to create private chat.\n");
}

void Console::ChatSession(const std::string& chatName)
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

void Console::OpenChatFlow()
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

void Console::OpenGeneralChatFlow() {
    ChatSession(GENERAL_CHAT_NAME);
}

void Console::ShowMyChatsFlow() const {
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

void Console::ShowAllUsersFlow() const {
    const auto userLogins = m_service.GetAllUserLogins();

    if (userLogins.empty()) {
        std::cout << "Users not found.\n";
        return;
    }

    std::cout << "\n==== ALL USER LOGINS ====\n";

    for (const auto& userLogin : userLogins) {
        std::cout << "- " << userLogin << "\n";
    }
}

void Console::ShowMainMenu() const {
    std::cout << "\n==== MAIN MENU ====\n"
              << "0 - Exit\n"
              << "1 - Registration\n"
              << "2 - Log in\n";
}

void Console::ShowUserMenu() const {
    std::cout << "\n==== USER MENU ====\n"
              << "0 - Log out\n"
              << "1 - Get list of my chats\n"
              << "2 - Create private chat\n"
              << "3 - Open private chat\n"
              << "4 - Open general chat\n"
              << "5 - Get list of user\n";
}

} // namespace console_chat
