#include "theme.h"

#include <QApplication>
#include <QStyle>

namespace console_chat::qt {

void ApplyTheme() {
    QApplication::setStyle("Fusion");
    QApplication::setPalette(QApplication::style()->standardPalette());
    qApp->setStyleSheet(
        "QMainWindow { background: #f3f5f4; }"
        "QWidget { font-size: 14px; }"
        "QToolBar { background: #ffffff; border-bottom: 1px solid #d5dbd7; spacing: 8px; padding: 8px; }"
        "QLineEdit, QSpinBox, QComboBox { min-height: 28px; padding: 3px 6px; }"
        "QPushButton { min-height: 28px; padding: 4px 12px; }"
        "QPushButton[primary=true] { background: #197854; color: white; border: 1px solid #146143; border-radius: 4px; }"
        "QPushButton[primary=true]:disabled { background: #becdc5; border-color: #becdc5; color: #526159; }"
        "QListWidget, QTableWidget, QTextBrowser { background: white; border: 1px solid #d5dbd7; }"
        "QListWidget::item { min-height: 34px; padding: 4px; }"
        "QListWidget::item:selected { background: #dceee4; color: #153b29; }"
        "QTabBar::tab { padding: 9px 18px; }"
        "QLabel#loginTitle { font-size: 24px; font-weight: 600; padding: 12px 0; }"
        "QLabel#chatTitle { font-size: 18px; font-weight: 600; }"
        "QLabel#chatParticipants, QLabel#chatAccess { color: #5d6761; }"
        "QLabel#loginError { color: #a33030; }"
    );
}

} // namespace console_chat::qt
