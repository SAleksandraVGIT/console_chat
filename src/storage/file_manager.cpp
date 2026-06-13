#include "console_chat/storage/file_manager.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <utility>


namespace fs = std::filesystem;

namespace console_chat::storage {

namespace {

bool EnsureParentDirectory(const std::string& filePath) {
    const auto parent = fs::path(filePath).parent_path();
    if (parent.empty()) {
        return true;
    }

    std::error_code ec;
    fs::create_directories(parent, ec);
    return !ec;
}

bool SetOwnerOnlyPermissions(const std::string& filePath) {
#ifdef _WIN32
    (void)filePath;
    return true;
#else
    std::error_code ec;
    fs::permissions(
        filePath,
        fs::perms::owner_read | fs::perms::owner_write,
        fs::perm_options::replace,
        ec);
    return !ec;
#endif
}

} // namespace

FileManager::FileManager(
    std::string usersFilePath,
    std::string chatsFilePath)
    : m_usersFilePath(std::move(usersFilePath))
    , m_chatsFilePath(std::move(chatsFilePath))
{}

bool FileManager::Initialize() {
    if (!EnsureParentDirectory(m_usersFilePath) || !EnsureParentDirectory(m_chatsFilePath)) {
        return false;
    }

    std::error_code usersError;
    std::error_code chatsError;
    const bool usersExist = fs::exists(m_usersFilePath, usersError);
    const bool chatsExist = fs::exists(m_chatsFilePath, chatsError);

    if (usersError || chatsError || usersExist != chatsExist) {
        return false;
    }

    core::ServiceState state;
    if (usersExist) {
        if (!ReadState(state)) {
            return false;
        }
    } else if (!WriteState(state)) {
        return false;
    }

    m_state = std::move(state);
    m_initialized = true;
    return true;
}

bool FileManager::Load(core::ServiceState& state) {
    if (!m_initialized) {
        return false;
    }

    state = m_state;
    return true;
}

bool FileManager::ReadState(core::ServiceState& state) const {
    std::ifstream usersIn(m_usersFilePath);
    std::ifstream chatsIn(m_chatsFilePath);
    if (!usersIn || !chatsIn) {
        return false;
    }

    core::ServiceState loaded;
    size_t usersCount = 0;
    if (!(usersIn >> usersCount)) {
        return false;
    }

    loaded.Users.reserve(usersCount);
    for (size_t i = 0; i < usersCount; ++i) {
        core::UserState user;
        if (!(usersIn >> std::quoted(user.Login)
                      >> std::quoted(user.Name)
                      >> std::quoted(user.PasswordHash)))
        {
            return false;
        }
        loaded.Users.emplace_back(std::move(user));
    }

    size_t chatsCount = 0;
    if (!(chatsIn >> chatsCount)) {
        return false;
    }

    loaded.Chats.reserve(chatsCount);
    for (size_t i = 0; i < chatsCount; ++i) {
        core::ChatState chat;
        int isPrivate = 0;
        if (!(chatsIn >> std::quoted(chat.Name) >> isPrivate) ||
            (isPrivate != 0 && isPrivate != 1))
        {
            return false;
        }
        chat.IsPrivate = isPrivate == 1;

        if (chat.IsPrivate &&
            !(chatsIn >> std::quoted(chat.Participants[0])
                       >> std::quoted(chat.Participants[1])))
        {
            return false;
        }

        size_t messagesCount = 0;
        if (!(chatsIn >> messagesCount)) {
            return false;
        }

        chat.Messages.reserve(messagesCount);
        for (size_t m = 0; m < messagesCount; ++m) {
            core::Message message;
            if (!(chatsIn >> std::quoted(message.Name) >> std::quoted(message.Text))) {
                return false;
            }
            chat.Messages.emplace_back(std::move(message));
        }
        loaded.Chats.emplace_back(std::move(chat));
    }

    state = std::move(loaded);
    return true;
}

bool FileManager::WriteState(const core::ServiceState& state) const {
    if (!m_initialized &&
        (!EnsureParentDirectory(m_usersFilePath) ||
         !EnsureParentDirectory(m_chatsFilePath)))
    {
        return false;
    }

    {
        std::ofstream usersOut(m_usersFilePath, std::ios::trunc);
        if (!usersOut) {
            return false;
        }

        usersOut << state.Users.size() << '\n';
        for (const auto& user : state.Users) {
            usersOut << std::quoted(user.Login) << ' '
                     << std::quoted(user.Name) << ' '
                     << std::quoted(user.PasswordHash) << '\n';
        }
        usersOut.flush();
        if (!usersOut) {
            return false;
        }
    }
    if (!SetOwnerOnlyPermissions(m_usersFilePath)) {
        return false;
    }

    {
        std::ofstream chatsOut(m_chatsFilePath, std::ios::trunc);
        if (!chatsOut) {
            return false;
        }

        chatsOut << state.Chats.size() << '\n';
        for (const auto& chat : state.Chats) {
            chatsOut << std::quoted(chat.Name) << ' ' << (chat.IsPrivate ? 1 : 0) << '\n';
            if (chat.IsPrivate) {
                chatsOut << std::quoted(chat.Participants[0]) << ' '
                         << std::quoted(chat.Participants[1]) << '\n';
            }

            chatsOut << chat.Messages.size() << '\n';
            for (const auto& message : chat.Messages) {
                chatsOut << std::quoted(message.Name) << ' '
                         << std::quoted(message.Text) << '\n';
            }
        }
        chatsOut.flush();
        if (!chatsOut) {
            return false;
        }
    }

    return SetOwnerOnlyPermissions(m_chatsFilePath);
}

bool FileManager::AddUser(const core::UserState& user) {
    if (!m_initialized) {
        return false;
    }

    const auto duplicate = std::find_if(
        m_state.Users.begin(),
        m_state.Users.end(),
        [&user](const core::UserState& stored) { return stored.Login == user.Login; });

    if (duplicate != m_state.Users.end()) {
        return false;
    }

    auto updated = m_state;
    updated.Users.push_back(user);
    if (!WriteState(updated)) {
        return false;
    }

    m_state = std::move(updated);
    return true;
}

bool FileManager::AddChat(const core::ChatState& chat) {
    if (!m_initialized) {
        return false;
    }

    const auto duplicate = std::find_if(
        m_state.Chats.begin(),
        m_state.Chats.end(),
        [&chat](const core::ChatState& stored) { return stored.Name == chat.Name; });
    
    if (duplicate != m_state.Chats.end()) {
        return false;
    }

    if (chat.IsPrivate) {
        const auto userExists = [this](const std::string& login) {
            return std::any_of(
                m_state.Users.begin(),
                m_state.Users.end(),
                [&login](const core::UserState& user) { return user.Login == login; });
        };

        if (chat.Participants[0] == chat.Participants[1] ||
            !userExists(chat.Participants[0]) ||
            !userExists(chat.Participants[1]))
        {
            return false;
        }
    }

    auto updated = m_state;
    updated.Chats.push_back(chat);
    if (!WriteState(updated)) {
        return false;
    }

    m_state = std::move(updated);
    return true;
}

bool FileManager::AddMessage(
    const std::string& chatName,
    const std::string& senderLogin,
    const core::Message& message)
{
    if (!m_initialized) {
        return false;
    }

    auto updated = m_state;
    const auto chat = std::find_if(
        updated.Chats.begin(),
        updated.Chats.end(),
        [&chatName](const core::ChatState& stored) { return stored.Name == chatName; });
    const bool userExists = std::any_of(
        updated.Users.begin(),
        updated.Users.end(),
        [&senderLogin](const core::UserState& user) { return user.Login == senderLogin; });
    
    if (chat == updated.Chats.end() ||
        !userExists ||
        chat->Messages.size() >= core::MAX_MESSAGES_PER_CHAT)
    {
        return false;
    }

    if (chat->IsPrivate &&
        chat->Participants[0] != senderLogin &&
        chat->Participants[1] != senderLogin)
    {
        return false;
    }

    chat->Messages.push_back(message);
    if (!WriteState(updated)) {
        return false;
    }

    m_state = std::move(updated);
    return true;
}

bool FileManager::Reset() {
    std::error_code usersError;
    std::error_code chatsError;
    fs::remove(m_usersFilePath, usersError);
    fs::remove(m_chatsFilePath, chatsError);
    if (usersError || chatsError) {
        return false;
    }

    m_state = {};
    m_initialized = false;
    return true;
}

} // namespace console_chat::storage
