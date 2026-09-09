#pragma once

#include "session_types.h"
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabBar;

namespace console_chat::qt {

class LoginPage final : public QWidget {
    Q_OBJECT
public:
    explicit LoginPage(const LoginRequest& defaults, QWidget* parent = nullptr);
    void SetBusy(bool busy);
    void SetError(const QString& text);
    void ClearPassword();

signals:
    void submitted(const LoginRequest& request);

private:
    void UpdateMode();
    void Submit();

private:
    QLineEdit* m_host;
    QSpinBox* m_port;
    QComboBox* m_role;
    QTabBar* m_mode;
    QLineEdit* m_login;
    QLineEdit* m_password;
    QLineEdit* m_name;
    QLabel* m_nameLabel;
    QLabel* m_error;
    QPushButton* m_submit;
};

} // namespace console_chat::qt
