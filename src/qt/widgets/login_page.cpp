#include "login_page.h"
#include "ui_helpers.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabBar>
#include <QVBoxLayout>

namespace console_chat::qt {

LoginPage::LoginPage(const LoginRequest& defaults, QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->addStretch();

    auto* form = new QWidget(this);
    form->setMaximumWidth(440);

    auto* layout = new QVBoxLayout(form);

    auto* title = new QLabel("Console Chat", form);
    title->setObjectName("loginTitle");
    layout->addWidget(title);
    m_mode = new QTabBar(form);
    m_mode->setObjectName("loginMode");
    m_mode->addTab("Sign in");
    m_mode->addTab("Register");
    layout->addWidget(m_mode);

    auto* fields = new QFormLayout;
    fields->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_host = new QLineEdit(defaults.host, form);
    m_host->setObjectName("serverHost");
    m_port = new QSpinBox(form);
    m_port->setObjectName("serverPort");
    m_port->setRange(1024, 49151);
    m_port->setValue(defaults.port);

    auto* address = new QHBoxLayout;
    address->addWidget(m_host, 1);
    address->addWidget(m_port);
    fields->addRow("Server IPv4", address);

    m_role = new QComboBox(form);
    m_role->setObjectName("loginRole");
    m_role->addItems({"User", "ADMIN"});
    m_role->setCurrentIndex(defaults.admin ? 1 : 0);
    fields->addRow("Account type", m_role);

    m_name = new QLineEdit(form);
    m_name->setObjectName("displayName");
    m_nameLabel = new QLabel("Display name", form);
    fields->addRow(m_nameLabel, m_name);

    m_login = new QLineEdit(form);
    m_login->setObjectName("login");
    fields->addRow("Login", m_login);

    m_password = new QLineEdit(form);
    m_password->setObjectName("password");
    m_password->setEchoMode(QLineEdit::Password);
    fields->addRow("Password", m_password);
    layout->addLayout(fields);

    m_error = new QLabel(form);
    m_error->setObjectName("loginError");
    m_error->setWordWrap(true);
    m_error->setTextFormat(Qt::PlainText);
    layout->addWidget(m_error);

    m_submit = new QPushButton(form);
    m_submit->setObjectName("signInButton");
    m_submit->setProperty("primary", true);
    m_submit->setIcon(Icon("network-connect", QStyle::SP_DialogOkButton));
    layout->addWidget(m_submit);

    outer->addWidget(form, 0, Qt::AlignHCenter);
    outer->addStretch(2);

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
