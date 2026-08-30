#include "console_chat/storage/mysql_manager.h"

#include "console_chat/core/chat_service.h"
#include "mysql_queries.h"

#include <charconv>
#include <cctype>
#include <exception>
#include <fstream>
#include <limits>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>


namespace console_chat::storage {

namespace {

namespace Sql = mysql_queries;

constexpr std::string_view NOT_INITIALIZED_ERROR =
    "MySQLManager is not initialized";

struct MySQLStatementDeleter {
    void operator()(MYSQL_STMT* statement) const noexcept {
        if (statement) {
            mysql_stmt_close(statement);
        }
    }
};

struct MySQLResultDeleter {
    void operator()(MYSQL_RES* result) const noexcept {
        if (result) {
            mysql_free_result(result);
        }
    }
};

using MySQLStatementPtr = std::unique_ptr<MYSQL_STMT, MySQLStatementDeleter>;
using MySQLResultPtr = std::unique_ptr<MYSQL_RES, MySQLResultDeleter>;

MySQLStatementPtr PrepareStatement(
    MYSQL* connection,
    std::string_view sql,
    std::string& error)
{
    MySQLStatementPtr statement(mysql_stmt_init(connection));
    if (!statement) {
        error = "Failed to initialize MySQL prepared statement";
        return nullptr;
    }

    if (mysql_stmt_prepare(statement.get(), sql.data(), sql.size()) != 0) {
        error = mysql_stmt_error(statement.get());
        return nullptr;
    }

    return statement;
}

void BindString(
    MYSQL_BIND& binding,
    const std::string& value,
    unsigned long& length)
{
    length = static_cast<unsigned long>(value.size());
    binding.buffer_type = MYSQL_TYPE_STRING;
    binding.buffer = const_cast<char*>(value.data());
    binding.buffer_length = length;
    binding.length = &length;
}

void BindUnsignedLongLong(MYSQL_BIND& binding, unsigned long long& value) {
    binding.buffer_type = MYSQL_TYPE_LONGLONG;
    binding.buffer = &value;
    binding.is_unsigned = true;
}

void BindLongLong(MYSQL_BIND& binding, long long& value) {
    binding.buffer_type = MYSQL_TYPE_LONGLONG;
    binding.buffer = &value;
}

bool ExecuteStatement(
    MYSQL_STMT* statement,
    MYSQL_BIND* bindings,
    std::string& error)
{
    if (bindings && mysql_stmt_bind_param(statement, bindings) != 0) {
        error = mysql_stmt_error(statement);
        return false;
    }

    if (mysql_stmt_execute(statement) != 0) {
        error = mysql_stmt_error(statement);
        return false;
    }

    return true;
}

MySQLStatementPtr ExecutePrepared(
    MYSQL* connection,
    std::string_view sql,
    MYSQL_BIND* bindings,
    std::string& error)
{
    auto statement = PrepareStatement(connection, sql, error);
    if (!statement || !ExecuteStatement(statement.get(), bindings, error)) {
        return nullptr;
    }
    return statement;
}

bool ExecuteQuery(MYSQL* connection, std::string_view sql, std::string& error) {
    if (mysql_real_query(connection, sql.data(), sql.size()) != 0) {
        error = mysql_error(connection);
        return false;
    }
    return true;
}

class Transaction {
public:
    Transaction(MYSQL* connection, std::string& error)
        : m_connection(connection)
        , m_active(ExecuteQuery(connection, Sql::START_TRANSACTION, error))
    {}

    ~Transaction() {
        if (m_active) {
            mysql_rollback(m_connection);
        }
    }

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    bool IsActive() const noexcept {
        return m_active;
    }

    bool Commit(std::string& error) {
        if (!m_active) {
            return false;
        }
        if (mysql_commit(m_connection) != 0) {
            error = mysql_error(m_connection);
            return false;
        }
        m_active = false;
        return true;
    }

private:
    MYSQL* m_connection;
    bool m_active;
};

MySQLResultPtr ExecuteSelect(
    MYSQL* connection,
    std::string_view sql,
    std::string& error)
{
    if (mysql_real_query(connection, sql.data(), sql.size()) != 0) {
        error = mysql_error(connection);
        return nullptr;
    }

    MySQLResultPtr result(mysql_store_result(connection));
    if (!result) {
        error = mysql_error(connection);
        if (error.empty()) {
            error = "MySQL SELECT did not return a result set";
        }
        return nullptr;
    }

    return result;
}

std::string ReadColumn(MYSQL_ROW row, const unsigned long* lengths, std::size_t index) {
    if (!row[index]) {
        return {};
    }
    return std::string(row[index], lengths[index]);
}

std::string_view Trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }

    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }

    return value;
}

template <typename Number>
bool ParseUnsigned(
    std::string_view value,
    Number& result,
    std::string_view key,
    std::string& error)
{
    unsigned long long parsed = 0;
    const auto [end, parseError] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed);

    if (parseError != std::errc{} || end != value.data() + value.size() ||
        parsed > static_cast<unsigned long long>(std::numeric_limits<Number>::max()))
    {
        error = "Invalid numeric value for MySQL config key: " + std::string(key);
        return false;
    }

    result = static_cast<Number>(parsed);
    return true;
}

} // namespace

bool LoadMySQLConfig(
    const std::string& filePath,
    MySQLConfig& config,
    std::string& error)
{
    std::ifstream input(filePath);
    if (!input) {
        error = "Failed to open MySQL config file: " + filePath;
        return false;
    }

    MySQLConfig loadedConfig;
    std::unordered_set<std::string> keys;
    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;
        const std::string_view trimmedLine = Trim(line);
        if (trimmedLine.empty() || trimmedLine.front() == '#') {
            continue;
        }

        const std::size_t separator = trimmedLine.find('=');
        if (separator == std::string_view::npos) {
            error = "Invalid MySQL config line " + std::to_string(lineNumber);
            return false;
        }

        const std::string key(Trim(trimmedLine.substr(0, separator)));
        const std::string value(Trim(trimmedLine.substr(separator + 1)));
        if (key.empty()) {
            error = "Empty MySQL config key at line " + std::to_string(lineNumber);
            return false;
        }

        if (!keys.insert(key).second) {
            error = "Duplicate MySQL config key: " + key;
            return false;
        }

        if (key == "host") {
            loadedConfig.Host = value;
        } else if (key == "port") {
            if (!ParseUnsigned(value, loadedConfig.Port, key, error)) {
                return false;
            }
        } else if (key == "user") {
            loadedConfig.User = value;
        } else if (key == "password") {
            loadedConfig.Password = value;
        } else if (key == "database") {
            loadedConfig.Database = value;
        } else if (key == "connect_timeout_seconds") {
            std::chrono::seconds::rep timeout = 0;
            if (!ParseUnsigned(value, timeout, key, error)) {
                return false;
            }
            loadedConfig.ConnectTimeout = std::chrono::seconds{timeout};
        } else if (key == "charset") {
            loadedConfig.Charset = value;
        } else {
            error = "Unknown MySQL config key: " + key;
            return false;
        }
    }

    if (!input.eof()) {
        error = "Failed to read MySQL config file: " + filePath;
        return false;
    }

    config = std::move(loadedConfig);
    error.clear();
    return true;
}

MySQLManager::MySQLManager(MySQLConfig config)
    : m_config(std::move(config))
{}

bool MySQLManager::Initialize() {
    m_lastError.clear();

    if (m_initialized && m_connection) {
        if (mysql_ping(m_connection.get()) == 0) {
            return true;
        }

        m_connection.reset();
        m_initialized = false;
    }

    if (!ValidateConfig()) {
        return false;
    }

    MySQLConnectionPtr connection(mysql_init(nullptr));

    if (!connection) {
        m_lastError = "Failed to initialize MySQL client handle";
        return false;
    }

    const auto connectTimeoutSeconds =
        static_cast<unsigned int>(m_config.ConnectTimeout.count());

    if (mysql_options(
            connection.get(),
            MYSQL_OPT_CONNECT_TIMEOUT,
            &connectTimeoutSeconds) != 0)
    {
        m_lastError = mysql_error(connection.get());
        return false;
    }

    if (!m_config.Charset.empty() &&
        mysql_options(
            connection.get(),
            MYSQL_SET_CHARSET_NAME,
            m_config.Charset.c_str()) != 0)
    {
        m_lastError = mysql_error(connection.get());
        return false;
    }

    if (!mysql_real_connect(
            connection.get(),
            m_config.Host.c_str(),
            m_config.User.c_str(),
            m_config.Password.c_str(),
            m_config.Database.c_str(),
            m_config.Port,
            nullptr,
            0))
    {
        m_lastError = mysql_error(connection.get());
        return false;
    }

    m_connection = std::move(connection);
    m_initialized = true;
    return true;
}

bool MySQLManager::ValidateConfig() {
    if (m_config.Host.empty()) {
        m_lastError = "MySQL host must not be empty";
        return false;
    }

    if (m_config.Port == 0) {
        m_lastError = "MySQL port must be greater than zero";
        return false;
    }

    if (m_config.User.empty()) {
        m_lastError = "MySQL user must not be empty";
        return false;
    }

    if (m_config.Database.empty()) {
        m_lastError = "MySQL database must not be empty";
        return false;
    }

    if (m_config.ConnectTimeout <= std::chrono::seconds::zero()) {
        m_lastError = "MySQL connection timeout must be greater than zero";
        return false;
    }

    if (m_config.ConnectTimeout.count() > std::numeric_limits<unsigned int>::max()) {
        m_lastError = "MySQL connection timeout is too large";
        return false;
    }

    if (m_config.Charset.empty()) {
        m_lastError = "MySQL charset must not be empty";
        return false;
    }

    return true;
}

bool MySQLManager::EnsureInitialized() {
    if (m_initialized && m_connection) {
        return true;
    }
    m_lastError = NOT_INITIALIZED_ERROR;
    return false;
}

const std::string& MySQLManager::GetLastError() const noexcept {
    return m_lastError;
}

bool MySQLManager::Load(core::ServiceState& state) {
    m_lastError.clear();
    if (!EnsureInitialized()) {
        return false;
    }

    core::ServiceState loaded;

    auto usersResult = ExecuteSelect(
        m_connection.get(), Sql::SELECT_USERS, m_lastError);
    if (!usersResult) {
        return false;
    }

    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(usersResult.get()))) {
        const unsigned long* lengths = mysql_fetch_lengths(usersResult.get());
        if (!lengths || !row[0] || !row[1] || !row[2] || !row[3] || !row[4]) {
            m_lastError = "MySQL returned an invalid user row";
            return false;
        }

        std::int64_t bannedUntilEpoch = 0;
        int bannedForever = 0;
        try {
            bannedUntilEpoch = std::stoll(ReadColumn(row, lengths, 3));
            bannedForever = std::stoi(ReadColumn(row, lengths, 4));
        } catch (const std::exception&) {
            m_lastError = "MySQL returned invalid user ban values";
            return false;
        }

        loaded.Users.push_back(core::UserState{
            ReadColumn(row, lengths, 0),
            ReadColumn(row, lengths, 1),
            ReadColumn(row, lengths, 2),
            bannedUntilEpoch,
            bannedForever != 0});
    }

    if (mysql_errno(m_connection.get()) != 0) {
        m_lastError = mysql_error(m_connection.get());
        return false;
    }

    auto chatsResult = ExecuteSelect(
        m_connection.get(), Sql::SELECT_CHATS, m_lastError);
    if (!chatsResult) {
        return false;
    }

    std::unordered_map<std::string, std::size_t> chatIndexes;
    while ((row = mysql_fetch_row(chatsResult.get()))) {
        const unsigned long* lengths = mysql_fetch_lengths(chatsResult.get());
        if (!lengths || !row[0] || (row[1] == nullptr) != (row[2] == nullptr)) {
            m_lastError = "MySQL returned an invalid chat row";
            return false;
        }

        core::ChatState chat;
        chat.Name = ReadColumn(row, lengths, 0);
        chat.IsPrivate = row[1] != nullptr;
        if (chat.IsPrivate) {
            chat.Participants = {
                ReadColumn(row, lengths, 1),
                ReadColumn(row, lengths, 2)};
        }

        const std::size_t index = loaded.Chats.size();
        if (!chatIndexes.emplace(chat.Name, index).second) {
            m_lastError = "MySQL returned duplicate chat names";
            return false;
        }
        loaded.Chats.push_back(std::move(chat));
    }

    if (mysql_errno(m_connection.get()) != 0) {
        m_lastError = mysql_error(m_connection.get());
        return false;
    }

    auto messagesResult = ExecuteSelect(
        m_connection.get(), Sql::SELECT_MESSAGES, m_lastError);
    if (!messagesResult) {
        return false;
    }

    while ((row = mysql_fetch_row(messagesResult.get()))) {
        const unsigned long* lengths = mysql_fetch_lengths(messagesResult.get());
        if (!lengths || !row[0] || !row[1] || !row[2]) {
            m_lastError = "MySQL returned an invalid message row";
            return false;
        }

        const std::string chatName = ReadColumn(row, lengths, 0);
        const auto chat = chatIndexes.find(chatName);
        if (chat == chatIndexes.end()) {
            m_lastError = "MySQL returned a message for an unknown chat";
            return false;
        }

        loaded.Chats[chat->second].Messages.push_back(core::Message{
            ReadColumn(row, lengths, 1),
            ReadColumn(row, lengths, 2)});
    }

    if (mysql_errno(m_connection.get()) != 0) {
        m_lastError = mysql_error(m_connection.get());
        return false;
    }

    state = std::move(loaded);
    return true;
}

bool MySQLManager::AddUser(const core::UserState& user) {
    m_lastError.clear();
    if (!EnsureInitialized()) {
        return false;
    }

    Transaction transaction(m_connection.get(), m_lastError);
    if (!transaction.IsActive()) {
        return false;
    }

    MYSQL_BIND userBindings[2]{};
    unsigned long nameLength = 0;
    unsigned long loginLength = 0;
    BindString(userBindings[0], user.Name, nameLength);
    BindString(userBindings[1], user.Login, loginLength);
    auto userStatement = ExecutePrepared(
        m_connection.get(), Sql::INSERT_USER, userBindings, m_lastError);
    if (!userStatement) {
        return false;
    }

    unsigned long long userId = mysql_stmt_insert_id(userStatement.get());
    if (userId == 0) {
        m_lastError = "MySQL did not return the inserted user id";
        return false;
    }

    MYSQL_BIND passwordBindings[2]{};
    unsigned long passwordLength = 0;
    BindUnsignedLongLong(passwordBindings[0], userId);
    BindString(passwordBindings[1], user.PasswordHash, passwordLength);
    if (!ExecutePrepared(
            m_connection.get(),
            Sql::INSERT_PASSWORD,
            passwordBindings,
            m_lastError))
    {
        return false;
    }

    return transaction.Commit(m_lastError);
}

bool MySQLManager::AddChat(const core::ChatState& chat) {
    m_lastError.clear();
    if (!EnsureInitialized()) {
        return false;
    }

    if (!chat.IsPrivate) {
        MYSQL_BIND bindings[1]{};
        unsigned long chatNameLength = 0;
        BindString(bindings[0], chat.Name, chatNameLength);
        return ExecutePrepared(
            m_connection.get(),
            Sql::INSERT_GENERAL_CHAT,
            bindings,
            m_lastError) != nullptr;
    }

    if (chat.Participants[0].empty() || chat.Participants[1].empty())
    {
        m_lastError = "Private chat participants are invalid";
        return false;
    }

    const bool hasAdminParticipant =
        chat.Participants[0] == core::ADMIN_SYSTEM_LOGIN ||
        chat.Participants[1] == core::ADMIN_SYSTEM_LOGIN;

    std::unique_ptr<Transaction> transaction;
    if (hasAdminParticipant) {
        transaction = std::make_unique<Transaction>(m_connection.get(), m_lastError);
        if (!transaction->IsActive()) {
            return false;
        }

        if (!ExecutePrepared(
                m_connection.get(),
                Sql::INSERT_ADMIN_USER,
                nullptr,
                m_lastError))
        {
            return false;
        }
    }

    MYSQL_BIND bindings[3]{};
    unsigned long chatNameLength = 0;
    unsigned long firstLoginLength = 0;
    unsigned long secondLoginLength = 0;
    BindString(bindings[0], chat.Name, chatNameLength);
    BindString(bindings[1], chat.Participants[0], firstLoginLength);
    BindString(bindings[2], chat.Participants[1], secondLoginLength);
    auto statement = ExecutePrepared(
        m_connection.get(), Sql::INSERT_PRIVATE_CHAT, bindings, m_lastError);
    if (!statement) {
        return false;
    }

    if (mysql_stmt_affected_rows(statement.get()) != 1) {
        m_lastError = "Private chat participants were not found";
        return false;
    }

    return !transaction || transaction->Commit(m_lastError);
}

bool MySQLManager::AddMessage(
    const std::string& chatName,
    const std::string& senderLogin,
    const core::Message& message,
    const size_t maxMessagesPerChat)
{
    m_lastError.clear();
    if (!EnsureInitialized()) {
        return false;
    }

    MYSQL_BIND bindings[4]{};
    unsigned long messageLength = 0;
    unsigned long chatNameLength = 0;
    unsigned long senderLoginLength = 0;
    unsigned long long messageLimit = maxMessagesPerChat;
    BindString(bindings[0], message.Text, messageLength);
    BindString(bindings[1], chatName, chatNameLength);
    BindString(bindings[2], senderLogin, senderLoginLength);
    BindUnsignedLongLong(bindings[3], messageLimit);
    auto statement = ExecutePrepared(
        m_connection.get(), Sql::INSERT_MESSAGE, bindings, m_lastError);
    if (!statement) {
        return false;
    }

    if (mysql_stmt_affected_rows(statement.get()) != 1) {
        m_lastError =
            "Message chat or sender was not found, access was denied, "
            "or the limit was reached";
        return false;
    }

    return true;
}

bool MySQLManager::AddAdminMessage(
    const std::string& chatName,
    const core::Message& message,
    const size_t maxMessagesPerChat)
{
    m_lastError.clear();
    if (!EnsureInitialized()) {
        return false;
    }

    Transaction transaction(m_connection.get(), m_lastError);
    if (!transaction.IsActive()) {
        return false;
    }

    if (!ExecutePrepared(
            m_connection.get(),
            Sql::INSERT_ADMIN_USER,
            nullptr,
            m_lastError))
    {
        return false;
    }

    MYSQL_BIND bindings[3]{};
    unsigned long messageLength = 0;
    unsigned long chatNameLength = 0;
    unsigned long long messageLimit = maxMessagesPerChat;
    BindString(bindings[0], message.Text, messageLength);
    BindString(bindings[1], chatName, chatNameLength);
    BindUnsignedLongLong(bindings[2], messageLimit);

    auto statement = ExecutePrepared(
        m_connection.get(), Sql::INSERT_ADMIN_MESSAGE, bindings, m_lastError);
    if (!statement) {
        return false;
    }

    if (mysql_stmt_affected_rows(statement.get()) != 1) {
        m_lastError = "Admin message chat was not found or the limit was reached";
        return false;
    }

    return transaction.Commit(m_lastError);
}

bool MySQLManager::UpdateUserBan(
    const std::string& login,
    const std::int64_t bannedUntilEpoch,
    const bool bannedForever)
{
    m_lastError.clear();
    if (!EnsureInitialized()) {
        return false;
    }

    MYSQL_BIND bindings[3]{};
    long long signedUntil = static_cast<long long>(bannedUntilEpoch);
    long long signedForever = bannedForever ? 1 : 0;
    unsigned long loginLength = 0;
    BindLongLong(bindings[0], signedUntil);
    BindLongLong(bindings[1], signedForever);
    BindString(bindings[2], login, loginLength);

    auto statement = ExecutePrepared(
        m_connection.get(), Sql::UPDATE_USER_BAN, bindings, m_lastError);
    if (!statement) {
        return false;
    }

    if (mysql_stmt_affected_rows(statement.get()) > 1) {
        m_lastError = "Unexpected number of users updated";
        return false;
    }

    return true;
}

bool MySQLManager::DeletePrivateChatsWithUser(const std::string& login) {
    m_lastError.clear();
    if (!EnsureInitialized()) {
        return false;
    }

    Transaction transaction(m_connection.get(), m_lastError);
    if (!transaction.IsActive()) {
        return false;
    }

    MYSQL_BIND messageBindings[1]{};
    unsigned long messageLoginLength = 0;
    BindString(messageBindings[0], login, messageLoginLength);
    if (!ExecutePrepared(
            m_connection.get(),
            Sql::DELETE_MESSAGES_FOR_PRIVATE_CHATS_WITH_USER,
            messageBindings,
            m_lastError))
    {
        return false;
    }

    MYSQL_BIND chatBindings[1]{};
    unsigned long chatLoginLength = 0;
    BindString(chatBindings[0], login, chatLoginLength);
    if (!ExecutePrepared(
            m_connection.get(),
            Sql::DELETE_PRIVATE_CHATS_WITH_USER,
            chatBindings,
            m_lastError))
    {
        return false;
    }

    return transaction.Commit(m_lastError);
}

bool MySQLManager::Reset() {
    m_lastError.clear();
    if (!m_initialized && !Initialize()) {
        return false;
    }

    Transaction transaction(m_connection.get(), m_lastError);
    if (!transaction.IsActive()) {
        return false;
    }

    for (const std::string_view sql : Sql::RESET_STATEMENTS) {
        if (!ExecuteQuery(m_connection.get(), sql, m_lastError)) {
            return false;
        }
    }

    return transaction.Commit(m_lastError);
}

} // namespace console_chat::storage
