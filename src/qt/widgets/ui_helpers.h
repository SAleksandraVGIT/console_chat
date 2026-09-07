#pragma once

#include <QApplication>
#include <QIcon>
#include <QStyle>
#include <QString>

namespace console_chat::qt {

inline QIcon Icon(const QString& name, QStyle::StandardPixmap fallback) {
    return QIcon::fromTheme(name, QApplication::style()->standardIcon(fallback));
}

inline bool ValidField(const QString& value) {
    return !value.trimmed().isEmpty() && !value.contains('\t') &&
        !value.contains('\n') && !value.contains('\r');
}

} // namespace console_chat::qt
