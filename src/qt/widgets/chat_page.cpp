#include "chat_page.h"
#include "ui_helpers.h"

#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <algorithm>

namespace console_chat::qt {

ChatPage::ChatPage(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName("workspaceTabs");
    layout->addWidget(m_tabs);

    auto* split = new QSplitter(this);
    split->setChildrenCollapsible(false);
    m_tabs->addTab(split, Icon("mail-message-new", QStyle::SP_FileDialogDetailedView), "Chats");

    auto* sidebar = new QWidget(split);
    sidebar->setMinimumWidth(180);

    auto* sideLayout = new QVBoxLayout(sidebar);
    m_filter = new QLineEdit(sidebar);
    m_filter->setObjectName("chatFilter");
    m_filter->setPlaceholderText("Search chats");
    m_filter->setClearButtonEnabled(true);
    sideLayout->addWidget(m_filter);
    m_chats = new QListWidget(sidebar);
    m_chats->setObjectName("chatList");
    m_chats->setWordWrap(true);
    sideLayout->addWidget(m_chats, 1);
    m_create = new QPushButton(Icon("list-add", QStyle::SP_FileDialogNewFolder), "New chat", sidebar);
    m_create->setObjectName("createChatButton");
    sideLayout->addWidget(m_create);

    auto* conversation = new QWidget(split);
    auto* chatLayout = new QVBoxLayout(conversation);

    auto* heading = new QHBoxLayout;
    m_title = new QLabel("GENERAL", conversation);
    m_title->setObjectName("chatTitle");
    m_title->setWordWrap(true);
    m_title->setTextFormat(Qt::PlainText);
    m_access = new QLabel(conversation);
    m_access->setObjectName("chatAccess");
    heading->addWidget(m_title, 1);
    heading->addWidget(m_access);
    chatLayout->addLayout(heading);
    m_participants = new QLabel(conversation);
    m_participants->setObjectName("chatParticipants");
    m_participants->setTextFormat(Qt::PlainText);
    m_participants->setWordWrap(true);
    chatLayout->addWidget(m_participants);
    m_messages = new QTextBrowser(conversation);
    m_messages->setObjectName("messageHistory");
    m_messages->setOpenLinks(false);
    m_messages->setOpenExternalLinks(false);
    chatLayout->addWidget(m_messages, 1);

    auto* input = new QHBoxLayout;
    m_composer = new QLineEdit(conversation);
    m_composer->setObjectName("messageInput");
    m_composer->setPlaceholderText("Message");
    m_send = new QPushButton(Icon("mail-send", QStyle::SP_ArrowForward), "Send", conversation);
    m_send->setObjectName("sendMessageButton");
    m_send->setProperty("primary", true);
    input->addWidget(m_composer, 1);
    input->addWidget(m_send);
    chatLayout->addLayout(input);
    split->addWidget(sidebar);
    split->addWidget(conversation);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({240, 720});

    auto* directory = new QWidget(this);

    auto* userLayout = new QVBoxLayout(directory);
    m_users = new QTableWidget(directory);
    m_users->setObjectName("userTable");
    m_users->setColumnCount(3);
    m_users->setHorizontalHeaderLabels({"Login", "Display name", "Account status"});
    m_users->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_users->verticalHeader()->hide();
    m_users->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_users->setSelectionMode(QAbstractItemView::SingleSelection);
    m_users->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_users->setAlternatingRowColors(true);
    userLayout->addWidget(m_users, 1);
    m_userChat = new QPushButton(Icon("mail-message-new", QStyle::SP_FileDialogNewFolder),
                               "Create chat with selected user", directory);
    m_userChat->setObjectName("userChatButton");
    userLayout->addWidget(m_userChat, 0, Qt::AlignLeft);
    m_adminActions = new QWidget(directory);

    auto* actions = new QHBoxLayout(m_adminActions);
    actions->setContentsMargins(0, 0, 0, 0);
    m_disconnect = new QPushButton("Disconnect", m_adminActions);
    m_disconnect->setObjectName("disconnectUserButton");
    m_disconnect->setIcon(Icon("network-disconnect", QStyle::SP_DialogCloseButton));
    m_ban = new QPushButton("Ban", m_adminActions);
    m_ban->setObjectName("banUserButton");
    m_ban->setIcon(Icon("user-lock", QStyle::SP_MessageBoxWarning));
    m_unban = new QPushButton("Unban", m_adminActions);
    m_unban->setObjectName("unbanUserButton");
    m_unban->setIcon(Icon("user-unlock", QStyle::SP_DialogApplyButton));
    m_cleanup = new QPushButton("Delete permanently banned accounts", m_adminActions);
    m_cleanup->setObjectName("cleanupAccountsButton");
    m_cleanup->setIcon(Icon("edit-delete", QStyle::SP_TrashIcon));
    actions->addWidget(m_disconnect);
    actions->addWidget(m_ban);
    actions->addWidget(m_unban);
    actions->addStretch();
    actions->addWidget(m_cleanup);
    userLayout->addWidget(m_adminActions);

    m_tabs->addTab(directory, Icon("system-users", QStyle::SP_DirHomeIcon), "Users");

    connect(m_filter, &QLineEdit::textChanged, this, &ChatPage::FilterChats);

    connect(m_chats, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (item && item->data(Qt::UserRole).toString() != m_state.selectedChat) {
            emit chatSelected(item->data(Qt::UserRole).toString());
        }
    });

    connect(m_composer, &QLineEdit::textChanged, this, &ChatPage::UpdateControls);

    connect(m_composer, &QLineEdit::returnPressed, this, &ChatPage::SubmitMessage);

    connect(m_send, &QPushButton::clicked, this, &ChatPage::SubmitMessage);

    connect(m_create, &QPushButton::clicked, this, [this] {
        emit createRequested({});
    });

    connect(m_userChat, &QPushButton::clicked, this, [this] {
        emit createRequested(SelectedUser());
    });

    connect(m_users, &QTableWidget::itemSelectionChanged, this, &ChatPage::UpdateControls);

    connect(m_disconnect, &QPushButton::clicked, this, [this] {
        emit moderateRequested(UserAction::Disconnect, SelectedUser());
    });

    connect(m_ban, &QPushButton::clicked, this, [this] {
        emit moderateRequested(UserAction::Ban, SelectedUser());
    });

    connect(m_unban, &QPushButton::clicked, this, [this] {
        emit moderateRequested(UserAction::Unban, SelectedUser());
    });

    connect(m_cleanup, &QPushButton::clicked, this, &ChatPage::cleanupRequested);
    UpdateControls();
}

void ChatPage::SetSnapshot(const SessionSnapshot& state) {
    const bool switched = state.selectedChat != m_state.selectedChat;
    if (!state.authenticated) {
        m_drafts.clear();
        m_composer->clear();
        m_filter->clear();
    } else if (switched) {
        if (!m_state.selectedChat.isEmpty()) {
            m_drafts[m_state.selectedChat] = m_composer->text();
        }
        m_composer->setText(m_drafts.value(state.selectedChat));
    }

    if (state.chats != m_state.chats) {
        const QSignalBlocker blocker(m_chats);
        const auto scroll = m_chats->verticalScrollBar()->value();
        m_chats->clear();

        for (const auto& chat : state.chats) {
            auto* item = new QListWidgetItem(chat.name, m_chats);
            item->setData(Qt::UserRole, chat.name);
            item->setToolTip(chat.participants + (chat.writable ? "" : " (read-only)"));
            item->setIcon(Icon(chat.writable ? "mail-message-new" : "document-preview",
                              QStyle::SP_FileIcon));
        }
        m_chats->verticalScrollBar()->setValue(scroll);
        FilterChats();
    }

    {
        const QSignalBlocker blocker(m_chats);
        for (int i = 0; i < m_chats->count(); ++i) {
            if (m_chats->item(i)->data(Qt::UserRole).toString() == state.selectedChat) {
                m_chats->setCurrentRow(i);
            }
        }
    }

    if (state.users != m_state.users || state.admin != m_state.admin || state.login != m_state.login) {
        const auto selection = SelectedUser();
        const auto scroll = m_users->verticalScrollBar()->value();
        const QSignalBlocker blocker(m_users);
        m_users->clearSelection();
        m_users->setCurrentCell(-1, -1);
        m_users->setRowCount(state.users.size());

        for (int row = 0; row < state.users.size(); ++row) {
            const auto& user = state.users[row];
            auto* login = new QTableWidgetItem(user.login + (user.login == state.login ? " (You)" : ""));
            login->setData(Qt::UserRole, user.login);
            m_users->setItem(row, 0, login);
            m_users->setItem(row, 1, new QTableWidgetItem(user.name));
            m_users->setItem(row, 2, new QTableWidgetItem(user.ban));

            if (user.login == selection) {
                m_users->selectRow(row);
            }
        }
        m_users->verticalScrollBar()->setValue(scroll);
    }

    m_users->setColumnHidden(1, !state.admin);
    m_users->setColumnHidden(2, !state.admin);
    if (switched || state.messages != m_state.messages) {
        auto* scroll = m_messages->verticalScrollBar();
        const int previous = scroll->value();
        const bool follow = switched || previous >= scroll->maximum() - 4;
        QString html;

        for (const auto& message : state.messages) {
            html += "<p><b>" + message.name.toHtmlEscaped() + "</b><br>" +
                    message.text.toHtmlEscaped() + "</p>";
        }
        m_messages->setHtml(html);
        scroll->setValue(follow ? scroll->maximum() : previous);
    }

    m_state = state;
    m_title->setText(state.selectedChat.isEmpty() ? "No chat selected" : state.selectedChat);
    m_participants->clear();
    for (const auto& chat : state.chats) {
        if (chat.name == state.selectedChat) {
            m_participants->setText(chat.participants);
        }
    }

    if (switched) {
        m_tabs->setCurrentIndex(0);
    }
    UpdateControls();
}

void ChatPage::FilterChats() {
    for (int i = 0; i < m_chats->count(); ++i) {
        m_chats->item(i)->setHidden(!m_chats->item(i)->text().contains(m_filter->text(), Qt::CaseInsensitive));
    }
}

void ChatPage::UpdateControls() {
    const auto chat = std::find_if(m_state.chats.begin(), m_state.chats.end(),
        [this](const ChatItem& item) {
            return item.name == m_state.selectedChat;
        });

    const bool writable = chat != m_state.chats.end() && chat->writable;
    m_access->setText(writable ? "" : "Read-only");
    m_composer->setEnabled(m_state.authenticated && writable);
    m_send->setEnabled(!m_busy && writable && ValidField(m_composer->text()));
    m_chats->setEnabled(!m_busy);
    m_create->setEnabled(!m_busy && m_state.authenticated);

    const bool selected = !SelectedUser().isEmpty() && !m_busy;
    m_userChat->setEnabled(selected);
    m_adminActions->setVisible(m_state.admin);
    m_disconnect->setEnabled(m_state.admin && selected);
    m_ban->setEnabled(m_state.admin && selected);
    m_unban->setEnabled(m_state.admin && selected);
    m_cleanup->setEnabled(m_state.admin && !m_busy);
}

void ChatPage::SetBusy(const bool busy)
{
    m_busy = busy;
    UpdateControls();
}

QString ChatPage::SelectedUser() const {
    const auto* item = m_users->item(m_users->currentRow(), 0);
    return item ? item->data(Qt::UserRole).toString() : QString{};
}

void ChatPage::SubmitMessage() {
    if (m_send->isEnabled()) {
        emit sendRequested(m_composer->text());
    }
}

void ChatPage::MessageSent(const QString& chat, const QString& text) {
    if (m_drafts.value(chat) == text) {
        m_drafts.remove(chat);
    }

    if (chat == m_state.selectedChat && m_composer->text() == text) {
        m_composer->clear();
    }
}

} // namespace console_chat::qt
