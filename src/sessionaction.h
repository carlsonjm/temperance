#pragma once
#include <QDBusMessage>
#include <QString>

// Every session action that closes apps asks first, through Plasma's own
// prompt: logging out ends the session's apps as surely as a restart does.
inline QDBusMessage sessionActionPrompt(const QString &action)
{
    QString method;
    if (action == QStringLiteral("logout")) method = QStringLiteral("promptLogout");
    else if (action == QStringLiteral("restart")) method = QStringLiteral("promptReboot");
    else if (action == QStringLiteral("shutdown")) method = QStringLiteral("promptShutDown");
    else return {};
    return QDBusMessage::createMethodCall(QStringLiteral("org.kde.LogoutPrompt"),
        QStringLiteral("/LogoutPrompt"), QStringLiteral("org.kde.LogoutPrompt"), method);
}
