#include "main_window.h"
#include "chat_page.h"
#include "login_page.h"
#include "ui_helpers.h"

#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

namespace console_chat::qt {

MainWindow::MainWindow(const LoginRequest& defaults, const int refreshIntervalMs, QWidget* parent)
    : QMainWindow(parent), m_controller(refreshIntervalMs) {
    setWindowTitle("Console Chat");
    setWindowIcon(Icon("mail-message-new", QStyle::SP_ComputerIcon));
    resize(1040, 720);
    setMinimumSize(760, 520);

    auto* toolbar = addToolBar("Session");
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_identity = new QLabel("Console Chat", toolbar);
    m_identity->setTextFormat(Qt::PlainText);
    m_identity->setWordWrap(true);
    m_identity->setMinimumWidth(100);
    m_identity->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    toolbar->addWidget(m_identity);
    m_refresh = toolbar->addAction(Icon("view-refresh", QStyle::SP_BrowserReload), "Refresh");
    m_refresh->setObjectName("refreshAction");
    m_refresh->setToolTip("Refresh chats and users");

    m_deleteAccount = toolbar->addAction(Icon("edit-delete", QStyle::SP_TrashIcon), "Delete account");
    m_deleteAccount->setObjectName("deleteAccountAction");

    m_logout = toolbar->addAction(Icon("system-log-out", QStyle::SP_DialogCloseButton), "Log out");
    m_logout->setObjectName("logoutAction");

    m_pages = new QStackedWidget(this);
    m_login = new LoginPage(defaults, m_pages);
    m_chat = new ChatPage(m_pages);
    m_pages->addWidget(m_login);
    m_pages->addWidget(m_chat);
    setCentralWidget(m_pages);

    m_status = new QLabel("Disconnected", this);
    m_status->setObjectName("connectionStatus");
    m_status->setTextFormat(Qt::PlainText);
    m_status->setWordWrap(true);
    statusBar()->addWidget(m_status, 1);

    m_progress = new QLabel(this);
    statusBar()->addPermanentWidget(m_progress);

    connect(m_login, &LoginPage::submitted, &m_controller, &SessionController::Login);

    connect(m_chat, &ChatPage::chatSelected, &m_controller, &SessionController::OpenChat);

    connect(m_chat, &ChatPage::sendRequested, &m_controller, &SessionController::SendMessage);

    connect(m_chat, &ChatPage::createRequested, this, &MainWindow::CreateChat);

    connect(m_chat, &ChatPage::moderateRequested, this, &MainWindow::Moderate);

    connect(m_chat, &ChatPage::cleanupRequested, this, &MainWindow::CleanupAccounts);

    connect(m_refresh, &QAction::triggered, &m_controller, &SessionController::Refresh);

    connect(m_logout, &QAction::triggered, &m_controller, &SessionController::Logout);

    connect(m_deleteAccount, &QAction::triggered, this, &MainWindow::DeleteAccount);

    connect(&m_controller, &SessionController::snapshotChanged, this, &MainWindow::SetSnapshot);

    connect(&m_controller, &SessionController::messageSent, m_chat, &ChatPage::MessageSent);

    connect(&m_controller, &SessionController::busyChanged, this, [this](bool busy) {
        m_login->SetBusy(busy);
        m_chat->SetBusy(busy);
        m_progress->setText(busy ? "Working..." : "");
        m_refresh->setEnabled(!busy && m_controller.State().authenticated);
        m_logout->setEnabled(!busy && m_controller.State().authenticated);
        m_deleteAccount->setEnabled(!busy && m_controller.State().authenticated);
    });

    connect(&m_controller, &SessionController::error, this, [this](const QString& text) {
        m_status->setText(text);
        if (!m_controller.State().authenticated) {
            m_login->SetError(text);
        }
    });

    connect(&m_controller, &SessionController::notice, this, [this](const QString& text) {
        m_status->setText(text);
        if (!m_controller.State().authenticated) {
            m_login->SetError(text);
        }
    });

    connect(&m_controller, &SessionController::chatAlreadyExists, this, [this](const QString& name) {
        auto* message = new QMessageBox(QMessageBox::Information, "Chat already exists",
            "A chat with this participant already exists:\n" + name, QMessageBox::Ok, this);
        message->setObjectName("existingChatMessage");
        message->setTextFormat(Qt::PlainText);
        message->setAttribute(Qt::WA_DeleteOnClose);
        connect(message, &QDialog::finished, this, [this, name] {
            if (m_controller.State().authenticated) {
                m_controller.OpenChat(name);
            }
        });
        connect(&m_controller, &SessionController::snapshotChanged, message,
            [message](const SessionSnapshot& state) {
                if (!state.authenticated) {
                    message->close();
                }
            });
        message->open();
    });

    connect(&m_controller, &SessionController::chatNameInUse, this, [this](const QString& name) {
        auto* message = new QMessageBox(QMessageBox::Warning, "Chat name already in use",
            "This chat name is already in use:\n" + name + "\nChoose a different name.", QMessageBox::Ok, this);
        message->setObjectName("chatNameInUseMessage");
        message->setTextFormat(Qt::PlainText);
        message->setAttribute(Qt::WA_DeleteOnClose);

        connect(&m_controller, &SessionController::snapshotChanged, message,
            [message](const SessionSnapshot& state) {
                if (!state.authenticated) {
                    message->close();
                }
            });
        message->open();
    });

    qApp->installEventFilter(this);
    SetSnapshot({});
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
}

bool MainWindow::eventFilter(QObject* object, QEvent* event) {
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::Wheel)
    {
        auto* widget = qobject_cast<QWidget*>(object);
        if (widget && (widget == this || isAncestorOf(widget))) {
            m_controller.RecordActivity();
        }
    }
    return QMainWindow::eventFilter(object, event);
}

void MainWindow::SetSnapshot(const SessionSnapshot& state) {
    const bool wasLoggedIn = m_pages->currentWidget() == m_chat;
    m_pages->setCurrentWidget(state.authenticated ? static_cast<QWidget*>(m_chat) : m_login);
    m_chat->SetSnapshot(state);
    m_identity->setText(state.authenticated ? (state.admin ? "ADMIN" : state.name + " (" + state.login + ")")
                                          : "Console Chat");

    m_deleteAccount->setVisible(state.authenticated && !state.admin);
    m_logout->setVisible(state.authenticated);
    m_refresh->setVisible(state.authenticated);

    if (state.authenticated != wasLoggedIn) {
        m_status->setText(state.authenticated ? "Connected" : "Disconnected");
        m_login->ClearPassword();
    }
}

void MainWindow::CreateChat(const QString& recipient) {
    QDialog dialog(this);
    dialog.setWindowTitle("New private chat");
    dialog.setMinimumWidth(380);

    auto* layout = new QVBoxLayout(&dialog);
    auto* fields = new QFormLayout;
    auto* users = new QComboBox(&dialog);
    users->setObjectName("newChatRecipient");

    for (const auto& user : m_controller.State().users) {
        users->addItem(user.login + (user.login == m_controller.State().login ? " (You)" : ""), user.login);
    }

    if (!recipient.isEmpty()) {
        users->setCurrentIndex(users->findData(recipient));
    }

    auto* name = new QLineEdit(&dialog);
    name->setObjectName("newChatName");
    fields->addRow("Participant", users);
    fields->addRow("Chat name", name);
    layout->addLayout(fields);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("Create chat");
    const auto validate = [=] {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(users->currentIndex() >= 0 && ValidField(name->text()));
    };

    connect(name, &QLineEdit::textChanged, &dialog, validate);

    connect(users, &QComboBox::currentIndexChanged, &dialog, validate);
    validate();

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    layout->addWidget(buttons);
    if (dialog.exec() == QDialog::Accepted) {
        m_controller.CreateChat(users->currentData().toString(), name->text().trimmed());
    }
}

void MainWindow::Moderate(const UserAction action, const QString& login) {
    if (login.isEmpty() || !m_controller.State().admin) {
        return;
    }

    QString period;
    if (action == UserAction::Ban) {
        QDialog dialog(this);
        dialog.setWindowTitle("Ban user");

        auto* layout = new QFormLayout(&dialog);
        auto* user = new QLabel(login, &dialog);
        user->setTextFormat(Qt::PlainText);
        user->setWordWrap(true);
        layout->addRow("Login", user);

        auto* periods = new QComboBox(&dialog);
        periods->setObjectName("banPeriod");
        periods->addItem("1 day", "1d");
        periods->addItem("10 days", "10d");
        periods->addItem("1 month", "1m");
        periods->addItem("1 year", "1y");
        periods->addItem("Forever", "forever");
        layout->addRow("Duration", periods);

        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        buttons->button(QDialogButtonBox::Ok)->setText("Ban user");
        layout->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted) {
            return;
        }
        period = periods->currentData().toString();
    }
    m_controller.Moderate(action, login, period);
}

void MainWindow::DeleteAccount() {
    bool accepted = false;
    const auto login = m_controller.State().login;
    const auto confirmation = QInputDialog::getText(this, "Delete account",
        "Your account and private chats will be deleted.\nEnter your login to confirm:",
        QLineEdit::Normal, {}, &accepted);

    if (!accepted) {
        return;
    }

    if (confirmation != login) {
        m_status->setText("Confirmation does not match your login.");
        return;
    }
    m_controller.DeleteAccount(confirmation);
}

void MainWindow::CleanupAccounts() {
    if (QMessageBox::warning(this, "Delete permanently banned accounts",
        "Delete all permanently banned accounts and their private chats?",
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Yes) {
        m_controller.DeleteForeverBanned();
    }
}

} // namespace console_chat::qt
