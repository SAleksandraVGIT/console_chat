#include "chat_page.h"
#include "widget_helpers.h"
#include "ui_chat_page.h"

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
#include <algorithm>

namespace console_chat::qt {

ChatPage::ChatPage(QWidget* parent) : QWidget(parent) {
    Ui::ChatPage ui;
    ui.setupUi(this);
    m_tabs = ui.workspaceTabs;
    m_filter = ui.chatFilter;
    m_chats = ui.chatList;
    m_create = ui.createChatButton;
    m_title = ui.chatTitle;
    m_access = ui.chatAccess;
    m_participants = ui.chatParticipants;
    m_messages = ui.messageHistory;
    m_composer = ui.messageInput;
    m_send = ui.sendMessageButton;
    m_users = ui.userTable;
    m_userChat = ui.userChatButton;
    m_adminActions = ui.adminActions;
    m_disconnect = ui.disconnectUserButton;
    m_ban = ui.banUserButton;
    m_unban = ui.unbanUserButton;
    m_cleanup = ui.cleanupAccountsButton;

    ui.chatSplitter->setStretchFactor(0, 0);
    ui.chatSplitter->setStretchFactor(1, 1);
    ui.chatSplitter->setSizes({240, 720});
    m_tabs->setTabIcon(0, Icon("mail-message-new", QStyle::SP_FileDialogDetailedView));
    m_tabs->setTabIcon(1, Icon("system-users", QStyle::SP_DirHomeIcon));
    m_users->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_create->setIcon(Icon("list-add", QStyle::SP_FileDialogNewFolder));
    m_send->setIcon(Icon("mail-send", QStyle::SP_ArrowForward));
    m_userChat->setIcon(Icon("mail-message-new", QStyle::SP_FileDialogNewFolder));
    m_disconnect->setIcon(Icon("network-disconnect", QStyle::SP_DialogCloseButton));
    m_ban->setIcon(Icon("user-lock", QStyle::SP_MessageBoxWarning));
    m_unban->setIcon(Icon("user-unlock", QStyle::SP_DialogApplyButton));
    m_cleanup->setIcon(Icon("edit-delete", QStyle::SP_TrashIcon));

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
