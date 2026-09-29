#pragma once
#include <QDBusMessage>
#include <QString>

// Logout explicitly skips the greeter; power actions always retain confirmation.
inline QDBusMessage sessionActionPrompt(const QString &action)
{
    QString method;
    if (action == QStringLiteral("logout")) {
        return QDBusMessage::createMethodCall(QStringLiteral("org.kde.Shutdown"),
            QStringLiteral("/Shutdown"), QStringLiteral("org.kde.Shutdown"), QStringLiteral("logout"));
    }
    else if (action == QStringLiteral("restart")) method = QStringLiteral("promptReboot");
    else if (action == QStringLiteral("shutdown")) method = QStringLiteral("promptShutDown");
    else return {};
    return QDBusMessage::createMethodCall(QStringLiteral("org.kde.LogoutPrompt"),
        QStringLiteral("/LogoutPrompt"), QStringLiteral("org.kde.LogoutPrompt"), method);
}
