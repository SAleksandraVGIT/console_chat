#include "session_controller.h"
#include "theme.h"
#include "widgets/main_window.h"
#include "widgets/chat_page.h"
#include "console_chat/client/chat_client.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTimer>
#include <memory>

using namespace console_chat::qt;
using console_chat::client::ChatClient;

class QtClientTest final : public QObject {
    Q_OBJECT
private:
    std::unique_ptr<QTemporaryDir> m_directory;
    QProcess m_server;
    int m_port = 0;

    bool StartServer(const int timeout = 15) {
        m_directory = std::make_unique<QTemporaryDir>();
        if (!m_directory->isValid()) {
            return false;
        }

        QTcpServer probe;
        for (int attempt = 0; attempt < 50; ++attempt) {
            m_port = 30000 + QRandomGenerator::global()->bounded(15000);
            if (probe.listen(QHostAddress::LocalHost, m_port)) {
                break;
            }
        }

        if (!probe.isListening()) {
            return false;
        }

        probe.close();
        const auto config = m_directory->filePath("server.conf");
        QFile limits(config);
        if (!limits.open(QIODevice::WriteOnly)) {
            return false;
        }
        limits.write("client_timeout_seconds=" + QByteArray::number(timeout) + "\n");
        limits.close();

        const auto admin = m_directory->filePath("admin.conf");
        QFile credentials(admin);
        if (!credentials.open(QIODevice::WriteOnly)) {
            return false;
        }

        credentials.write("login=operator\npassword=test-secret\n");
        credentials.close();
        m_server.start(CHAT_SERVER_PATH, {"--port", QString::number(m_port),
            "--users-file", m_directory->filePath("users.db"),
            "--chats-file", m_directory->filePath("chats.db"),
            "--admin-config", admin, "--server-config", config});

        if (!m_server.waitForStarted(3000)) {
            return false;
        }

        for (int attempt = 0; attempt < 50; ++attempt) {
            QTcpSocket socket;
            socket.connectToHost(QHostAddress::LocalHost, m_port);
            if (socket.waitForConnected(100)) {
                return true;
            }
            QTest::qWait(20);
        }
        return false;
    }

    LoginRequest Login(const QString& login, const bool admin = false) const {
        LoginRequest request;
        request.port = m_port;
        request.login = login;
        request.password = "test-secret";
        request.admin = admin;
        return request;
    }

    bool ContainsChat(const SessionController& controller, const QString& name, const bool writable) const {
        for (const auto& chat : controller.State().chats) {
            if (chat.name == name && chat.writable == writable) {
                return true;
            }
        }
        return false;
    }

    void SignInWindow(MainWindow& window, const QString& login) {
        window.findChild<QLineEdit*>("login")->setText(login);
        window.findChild<QLineEdit*>("password")->setText("test-secret");
        window.findChild<QPushButton*>("signInButton")->click();
    }

    bool CreateChatInWindow(MainWindow& window, const QString& recipient, const QString& name) {
        bool submitted = false;
        QTimer::singleShot(0, &window, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) {
                return;
            }

            auto* users = dialog->findChild<QComboBox*>("newChatRecipient");
            auto* chatName = dialog->findChild<QLineEdit*>("newChatName");
            auto* buttons = dialog->findChild<QDialogButtonBox*>();
            if (users && chatName && buttons) {
                users->setCurrentIndex(users->findData(recipient));
                chatName->setText(name);
                submitted = buttons->button(QDialogButtonBox::Ok)->isEnabled();
                if (submitted) {
                    buttons->button(QDialogButtonBox::Ok)->click();
                    return;
                }
            }
            dialog->reject();
        });
        window.findChild<QPushButton*>("createChatButton")->click();
        return submitted;
    }

private slots:
    void initTestCase()
    {
        ApplyTheme();
    }

    void cleanup() {
        if (m_server.state() != QProcess::NotRunning) {
            m_server.terminate();
            if (!m_server.waitForFinished(2000)) {
                m_server.kill();
                m_server.waitForFinished(2000);
            }
        }
        m_directory.reset();
    }

    void mixedClientsAndAdminPermissions() {
        QVERIFY(StartServer());

        ChatClient console("127.0.0.1", m_port, std::chrono::seconds{2});
        QVERIFY(console.Register("Alice", "alice", "test-secret"));
        QVERIFY(console.Register("Bob", "bob", "test-secret"));
        QVERIFY(console.Authenticate("alice", "test-secret"));

        SessionController user(100), admin(100);

        QSignalSpy existingChats(&user, &SessionController::chatAlreadyExists);
        QSignalSpy adminErrors(&admin, &SessionController::error);
        user.Login(Login("bob"));
        admin.Login(Login("operator", true));
        QTRY_VERIFY(user.State().authenticated);
        QTRY_VERIFY(admin.State().authenticated);

        user.CreateChat("alice", "Shared");
        QTRY_COMPARE(user.State().selectedChat, QString("Shared"));

        user.SendMessage("Hello from Qt");
        QTRY_COMPARE(console.GetMessages("Shared").size(), std::size_t{1});
        QVERIFY(console.SendMessage("Shared", "Hello from console"));
        QTRY_COMPARE(user.State().messages.size(), 2);
        QCOMPARE(user.State().messages.back().text, QString("Hello from console"));
        QTRY_VERIFY(ContainsChat(admin, "Shared", false));

        admin.OpenChat("Shared");
        QTRY_COMPARE(admin.State().messages.size(), 2);

        admin.SendMessage("Not allowed");
        QTRY_COMPARE(adminErrors.count(), 1);
        QCOMPARE(console.GetMessages("Shared").size(), std::size_t{2});

        admin.CreateChat("alice", "Admin room");
        QTRY_COMPARE(admin.State().selectedChat, QString("Admin room"));
        QVERIFY(ContainsChat(admin, "Admin room", true));

        admin.SendMessage("Private from ADMIN");
        QTRY_COMPARE(console.GetMessages("Admin room").size(), std::size_t{1});
        QCOMPARE(QString::fromStdString(console.GetMessages("Admin room")[0].Name), QString("ADMIN"));

        admin.OpenChat("GENERAL");
        QTRY_COMPARE(admin.State().selectedChat, QString("GENERAL"));

        admin.SendMessage("General from ADMIN");
        QTRY_COMPARE(console.GetMessages("GENERAL").size(), std::size_t{1});

        user.CreateChat("bob", "Notes");
        QTRY_COMPARE(user.State().selectedChat, QString("Notes"));

        user.CreateChat("bob", "Other name");
        QTRY_COMPARE(existingChats.count(), 1);
        QCOMPARE(existingChats.back()[0].toString(), QString("Notes"));
        QCOMPARE(user.State().selectedChat, QString("Notes"));

        user.OpenChat("GENERAL");

        user.SendMessage("Queued after opening GENERAL");
        QTRY_COMPARE(console.GetMessages("GENERAL").size(), std::size_t{2});
        QCOMPARE(QString::fromStdString(console.GetMessages("GENERAL").back().Text),
                 QString("Queued after opening GENERAL"));
    }

    void existingChatDialog_data() {
        QTest::addColumn<bool>("admin");
        QTest::addColumn<bool>("selfChat");
        QTest::addColumn<bool>("closeWindow");
        QTest::addColumn<bool>("disconnect");
        QTest::newRow("user") << false << false << false << false;
        QTest::newRow("admin") << true << false << false << false;
        QTest::newRow("self-chat") << false << true << false << false;
        QTest::newRow("close-button") << false << false << true << false;
        QTest::newRow("disconnected") << false << false << false << true;
    }

    void existingChatDialog() {
        QFETCH(bool, admin);
        QFETCH(bool, selfChat);
        QFETCH(bool, closeWindow);
        QFETCH(bool, disconnect);
        QVERIFY(StartServer());

        ChatClient console("127.0.0.1", m_port, std::chrono::seconds{2});
        QVERIFY(console.Register("Alice", "alice", "test-secret"));
        QVERIFY(console.Register("Bob", "bob", "test-secret"));
        const QString name = "<b>Shared & notes</b>";
        const QString recipient = selfChat ? "alice" : "bob";
        if (admin) {
            QVERIFY(console.AdminLogin("operator", "test-secret"));
            QVERIFY(console.AdminCreatePrivateChat(recipient.toStdString(), name.toStdString()));
            QVERIFY(console.AdminSendMessageToChat(name.toStdString(), "Existing history"));
        } else {
            QVERIFY(console.Authenticate("alice", "test-secret"));
            QVERIFY(console.CreatePrivateChat(recipient.toStdString(), name.toStdString()));
            QVERIFY(console.SendMessage(name.toStdString(), "Existing history"));
        }

        const QString login = admin ? "operator" : "alice";
        MainWindow window(Login(login, admin), 100);
        window.show();
        SignInWindow(window, login);
        QTRY_VERIFY(window.findChild<ChatPage*>()->isVisible());
        auto* create = window.findChild<QPushButton*>("createChatButton");
        QTRY_VERIFY(create->isEnabled());
        auto* title = window.findChild<QLabel*>("chatTitle");
        QCOMPARE(title->text(), QString("GENERAL"));
        auto* input = window.findChild<QLineEdit*>("messageInput");
        input->setText("General draft");

        QVERIFY(CreateChatInWindow(window, recipient, "Another name"));
        QTRY_VERIFY(window.findChild<QMessageBox*>("existingChatMessage"));
        QPointer<QMessageBox> message = window.findChild<QMessageBox*>("existingChatMessage");
        QVERIFY(message->isVisible());
        QCOMPARE(message->icon(), QMessageBox::Information);
        QCOMPARE(message->textFormat(), Qt::PlainText);
        QCOMPARE(message->text(), "A chat with this participant already exists:\n" + name);
        QCOMPARE(title->text(), QString("GENERAL"));
        QCOMPARE(input->text(), QString("General draft"));

        if (disconnect) {
            ChatClient moderator("127.0.0.1", m_port, std::chrono::seconds{2});
            QVERIFY(moderator.AdminLogin("operator", "test-secret"));
            QVERIFY(moderator.AdminKickUser("alice"));
            QTRY_VERIFY(!window.findChild<ChatPage*>()->isVisible());
            QTRY_VERIFY(message.isNull());
            return;
        }

        if (closeWindow) {
            message->close();
        } else {
            message->button(QMessageBox::Ok)->click();
        }

        QTRY_COMPARE(title->text(), name);
        QTRY_VERIFY(window.findChild<QTextBrowser*>("messageHistory")->toPlainText().contains("Existing history"));
        QVERIFY(input->isEnabled());
        QVERIFY(input->text().isEmpty());

        auto* chats = window.findChild<QListWidget*>("chatList");
        QCOMPARE(chats->count(), 2);
        QCOMPARE(chats->currentItem()->text(), name);
        const auto general = chats->findItems("GENERAL", Qt::MatchExactly);
        QCOMPARE(general.size(), 1);

        chats->setCurrentItem(general.front());
        QTRY_COMPARE(title->text(), QString("GENERAL"));
        QCOMPARE(input->text(), QString("General draft"));
    }

    void occupiedChatNameDialog_data() {
        QTest::addColumn<bool>("admin");
        QTest::addColumn<bool>("foreignChat");
        QTest::addColumn<bool>("general");

        QTest::newRow("user-visible-chat") << false << false << false;
        QTest::newRow("user-hidden-chat") << false << true << false;
        QTest::newRow("admin") << true << false << false;
        QTest::newRow("general") << false << false << true;
    }

    void occupiedChatNameDialog() {
        QFETCH(bool, admin);
        QFETCH(bool, foreignChat);
        QFETCH(bool, general);

        QVERIFY(StartServer());

        ChatClient console("127.0.0.1", m_port, std::chrono::seconds{2});
        QVERIFY(console.Register("Alice", "alice", "test-secret"));
        QVERIFY(console.Register("Bob", "bob", "test-secret"));
        QVERIFY(console.Register("Carol", "carol", "test-secret"));

        const QString name = general ? "GENERAL" : "<b>Occupied & private</b>";
        if (admin) {
            QVERIFY(console.AdminLogin("operator", "test-secret"));
            QVERIFY(console.AdminCreatePrivateChat("bob", name.toStdString()));
        } else if (!general) {
            QVERIFY(console.Authenticate(foreignChat ? "bob" : "alice", "test-secret"));
            QVERIFY(console.CreatePrivateChat(foreignChat ? "carol" : "bob", name.toStdString()));
        }

        const QString login = admin ? "operator" : "alice";
        MainWindow window(Login(login, admin), 100);
        window.show();
        SignInWindow(window, login);
        QTRY_VERIFY(window.findChild<ChatPage*>()->isVisible());

        auto* create = window.findChild<QPushButton*>("createChatButton");
        QTRY_VERIFY(create->isEnabled());
        auto* title = window.findChild<QLabel*>("chatTitle");
        auto* chats = window.findChild<QListWidget*>("chatList");
        auto* input = window.findChild<QLineEdit*>("messageInput");
        const int chatCount = chats->count();
        input->setText("Keep this draft");

        const QString recipient = foreignChat ? "bob" : "carol";
        QVERIFY(CreateChatInWindow(window, recipient, name));
        QTRY_VERIFY(window.findChild<QMessageBox*>("chatNameInUseMessage"));

        auto* message = window.findChild<QMessageBox*>("chatNameInUseMessage");
        QVERIFY(message->isVisible());
        QCOMPARE(message->icon(), QMessageBox::Warning);
        QCOMPARE(message->textFormat(), Qt::PlainText);
        QCOMPARE(message->text(), "This chat name is already in use:\n" + name + "\nChoose a different name.");
        QVERIFY(!window.findChild<QMessageBox*>("existingChatMessage"));

        message->button(QMessageBox::Ok)->click();
        QTRY_VERIFY(create->isEnabled());
        QCOMPARE(title->text(), QString("GENERAL"));
        QCOMPARE(input->text(), QString("Keep this draft"));
        QCOMPARE(chats->count(), chatCount);

        QVERIFY(CreateChatInWindow(window, recipient, "Available name"));
        QTRY_COMPARE(title->text(), QString("Available name"));
        QCOMPARE(chats->count(), chatCount + 1);
        QVERIFY(console.AdminLogin("operator", "test-secret"));

        const auto allChats = console.AdminGetChats();
        QCOMPARE(allChats.size(), general ? std::size_t{2} : std::size_t{3});
    }

    void bansUnbanAndExplicitCleanup() {
        QVERIFY(StartServer());

        SessionController user(100), admin(100);

        auto registration = Login("bob");
        registration.registration = true;
        registration.name = "Bob";

        user.Login(registration);
        admin.Login(Login("operator", true));
        QTRY_VERIFY(user.State().authenticated);
        QTRY_VERIFY(admin.State().authenticated);

        user.CreateChat("bob", "Notes");
        QTRY_VERIFY(ContainsChat(admin, "Notes", false));
        QSignalSpy errors(&user, &SessionController::error);

        admin.Moderate(UserAction::Ban, "bob", "1d");
        QTRY_VERIFY(!user.State().authenticated);
        QVERIFY(!admin.State().users.isEmpty());
        QVERIFY(ContainsChat(admin, "Notes", false));

        user.Login(Login("bob"));
        QTRY_VERIFY(errors.count() >= 2);
        QVERIFY(errors.back()[0].toString().contains("remaining"));

        admin.Moderate(UserAction::Unban, "bob");
        QTRY_COMPARE(admin.State().users.front().ban, QString("Active"));

        user.Login(Login("bob"));
        QTRY_VERIFY(user.State().authenticated);

        admin.Moderate(UserAction::Ban, "bob", "forever");
        QTRY_VERIFY(!user.State().authenticated);
        QTRY_COMPARE(admin.State().users.front().ban, QString("Banned forever"));
        QVERIFY(ContainsChat(admin, "Notes", false));

        admin.DeleteForeverBanned();
        QTRY_VERIFY(admin.State().users.isEmpty());
        QVERIFY(!ContainsChat(admin, "Notes", false));
    }

    void accountDeletionRequiresConfirmation() {
        QVERIFY(StartServer());

        SessionController user(100);

        auto request = Login("alice");
        request.registration = true;
        request.name = "Alice";

        user.Login(request);
        QTRY_VERIFY(user.State().authenticated);
        QSignalSpy errors(&user, &SessionController::error);

        user.DeleteAccount("wrong");
        QTRY_COMPARE(errors.count(), 1);
        QVERIFY(user.State().authenticated);

        user.DeleteAccount("alice");
        QTRY_VERIFY(!user.State().authenticated);
        ChatClient console("127.0.0.1", m_port, std::chrono::seconds{2});
        QVERIFY(!console.Authenticate("alice", "test-secret"));
    }

    void pollingDoesNotCancelTimeoutButTypingExtendsIt() {
        QVERIFY(StartServer(2));

        SessionController user(100);

        auto request = Login("alice");
        request.registration = true;
        request.name = "Alice";

        QSignalSpy errors(&user, &SessionController::error);
        user.Login(request);
        QTRY_VERIFY(user.State().authenticated);
        QTest::qWait(1200);

        user.RecordActivity();
        QTest::qWait(1200);
        QVERIFY(user.State().authenticated);
        QTRY_VERIFY_WITH_TIMEOUT(!user.State().authenticated, 3500);
        QVERIFY(!errors.isEmpty());
        QVERIFY(errors.back()[0].toString().contains("inactivity timeout"));
    }

    void stalledServerDoesNotBlockGui() {
        QTcpServer stalled;
        QVERIFY(stalled.listen(QHostAddress::LocalHost, 0));

        SessionController user(100);
        auto request = Login("alice");
        request.port = stalled.serverPort();

        QSignalSpy errors(&user, &SessionController::error);
        user.Login(request);

        int ticks = 0;
        QTimer timer;
        connect(&timer, &QTimer::timeout, [&] {
            ++ticks;
        });

        timer.start(20);
        QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(), 7000);
        QVERIFY(ticks > 20);
        QVERIFY(!user.State().authenticated);
    }

    void widgetsPreserveDraftAndShowRoleSpecificActions() {
        QVERIFY(StartServer());

        ChatClient console("127.0.0.1", m_port, std::chrono::seconds{2});

        QVERIFY(console.Register("Alice", "alice", "test-secret"));
        QVERIFY(console.Register("Bob", "bob", "test-secret"));
        QVERIFY(console.Authenticate("bob", "test-secret"));
        QVERIFY(console.CreatePrivateChat("alice", "Release notes"));
        QVERIFY(console.SendMessage("Release notes", "The review is ready."));

        auto defaults = Login("alice");
        MainWindow window(defaults, 100);
        window.show();

        QVERIFY(QDir().mkpath(QT_SCREENSHOTS_DIR));
        QVERIFY(window.grab().save(QString(QT_SCREENSHOTS_DIR) + "/login.png"));
        SignInWindow(window, "alice");

        auto* page = window.findChild<ChatPage*>();
        QTRY_VERIFY(page->isVisible());

        auto* input = window.findChild<QLineEdit*>("messageInput");
        auto* history = window.findChild<QTextBrowser*>("messageHistory");
        QTRY_VERIFY(input->isEnabled());
        input->setText("Unfinished draft");
        QVERIFY(console.SendMessage("GENERAL", "The release notes are ready for review."));
        QTRY_VERIFY(history->toPlainText().contains("ready for review"));
        QCOMPARE(input->text(), QString("Unfinished draft"));
        QVERIFY(!window.findChild<QPushButton*>("banUserButton")->isVisible());
        QVERIFY(window.findChild<QAction*>("deleteAccountAction")->isVisible());
        QVERIFY(window.grab().save(QString(QT_SCREENSHOTS_DIR) + "/user-chat.png"));
        window.resize(760, 520);

        QTest::qWait(50);
        QVERIFY(window.grab().save(QString(QT_SCREENSHOTS_DIR) + "/user-compact.png"));
        defaults.admin = true;
        MainWindow admin(defaults, 100);
        admin.show();
        SignInWindow(admin, "operator");
        QTRY_VERIFY(admin.findChild<ChatPage*>()->isVisible());

        auto* chats = admin.findChild<QListWidget*>("chatList");
        QTRY_VERIFY(chats->isEnabled());

        const auto matches = chats->findItems("Release notes", Qt::MatchExactly);
        QCOMPARE(matches.size(), 1);

        chats->setCurrentItem(matches.front());
        QTRY_COMPARE(admin.findChild<QLabel*>("chatTitle")->text(), QString("Release notes"));
        QVERIFY(!admin.findChild<QLineEdit*>("messageInput")->isEnabled());
        QCOMPARE(admin.findChild<QLabel*>("chatAccess")->text(), QString("Read-only"));

        admin.findChild<QTabWidget*>("workspaceTabs")->setCurrentIndex(1);
        QVERIFY(admin.findChild<QPushButton*>("banUserButton")->isVisible());
        QVERIFY(!admin.findChild<QAction*>("deleteAccountAction")->isVisible());
        QVERIFY(admin.grab().save(QString(QT_SCREENSHOTS_DIR) + "/admin-users.png"));
    }

    void removedUserDoesNotSelectAnotherModerationTarget() {
        ChatPage page;
        SessionSnapshot state;
        state.authenticated = true;
        state.admin = true;
        state.login = "ADMIN";
        state.users = {{"alice", "Alice", "Active"}, {"bob", "Bob", "Active"}};
        page.SetSnapshot(state);

        auto* table = page.findChild<QTableWidget*>("userTable");
        table->selectRow(0);
        QCOMPARE(page.SelectedUser(), QString("alice"));

        state.users.removeFirst();
        page.SetSnapshot(state);
        QVERIFY(page.SelectedUser().isEmpty());
        QVERIFY(!page.findChild<QPushButton*>("banUserButton")->isEnabled());
    }
};

QTEST_MAIN(QtClientTest)
#include "client_ui.moc"
