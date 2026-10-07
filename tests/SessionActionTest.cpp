#include "sessionaction.h"
#include <cstdlib>
int main()
{
    const QString actions[]{QStringLiteral("logout"), QStringLiteral("restart"), QStringLiteral("shutdown")};
    const QString methods[]{QStringLiteral("promptLogout"), QStringLiteral("promptReboot"), QStringLiteral("promptShutDown")};
    for (int i = 0; i < 3; ++i) {
        const auto message = sessionActionPrompt(actions[i]);
        if (message.service() != QStringLiteral("org.kde.LogoutPrompt")
            || message.path() != QStringLiteral("/LogoutPrompt")
            || message.interface() != QStringLiteral("org.kde.LogoutPrompt")
            || message.member() != methods[i] || !message.arguments().isEmpty()) return EXIT_FAILURE;
    }
    return sessionActionPrompt(QStringLiteral("invalid")).type() == QDBusMessage::InvalidMessage ? EXIT_SUCCESS : EXIT_FAILURE;
}
