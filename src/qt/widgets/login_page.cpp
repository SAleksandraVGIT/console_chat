#include "login_page.h"
#include "widget_helpers.h"
#include "ui_login_page.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabBar>

namespace console_chat::qt {

LoginPage::LoginPage(const LoginRequest& defaults, QWidget* parent) : QWidget(parent) {
    Ui::LoginPage ui;
    ui.setupUi(this);
    m_host = ui.serverHost;
    m_port = ui.serverPort;
    m_role = ui.loginRole;
    m_mode = ui.loginMode;
    m_name = ui.displayName;
    m_nameLabel = ui.displayNameLabel;
    m_login = ui.login;
    m_password = ui.password;
    m_error = ui.loginError;
    m_submit = ui.signInButton;

    m_mode->addTab("Sign in");
    m_mode->addTab("Register");
    m_host->setText(defaults.host);
    m_port->setValue(defaults.port);
    m_role->setCurrentIndex(defaults.admin ? 1 : 0);
    m_submit->setIcon(Icon("network-connect", QStyle::SP_DialogOkButton));

    connect(m_mode, &QTabBar::currentChanged, this, &LoginPage::UpdateMode);

    connect(m_role, &QComboBox::currentIndexChanged, this, &LoginPage::UpdateMode);

    connect(m_submit, &QPushButton::clicked, this, &LoginPage::Submit);

    connect(m_password, &QLineEdit::returnPressed, this, &LoginPage::Submit);
    UpdateMode();
}

void LoginPage::UpdateMode() {
    const bool admin = m_role->currentIndex() == 1;
    if (admin) {
        m_mode->setCurrentIndex(0);
    }

    m_mode->setTabVisible(1, !admin);
    const bool registration = !admin && m_mode->currentIndex() == 1;

    m_name->setVisible(registration);
    m_nameLabel->setVisible(registration);

    m_submit->setText(registration ? "Create account" : "Sign in");
    m_error->clear();
}

void LoginPage::Submit() {
    LoginRequest request;
    request.host = m_host->text().trimmed();
    request.port = m_port->value();
    request.admin = m_role->currentIndex() == 1;
    request.registration = m_mode->currentIndex() == 1;
    request.login = m_login->text().trimmed();
    request.password = m_password->text();
    request.name = m_name->text().trimmed();

    if (!ValidField(request.host) || !ValidField(request.login) || !ValidField(request.password) ||
        (request.registration && !ValidField(request.name))) {
        SetError("Complete all fields. Tabs and line breaks are not allowed.");
        return;
    }
    m_error->clear();
    emit submitted(request);
}

void LoginPage::SetBusy(const bool busy) {
    const QList<QWidget*> fields{m_host, m_port, m_role, m_mode, m_name, m_login, m_password, m_submit};
    for (auto* field : fields) {
        field->setEnabled(!busy);
    }
}

void LoginPage::SetError(const QString& text)
{
    m_error->setText(text);
}

void LoginPage::ClearPassword()
{
    m_password->clear();
}

} // namespace console_chat::qt
