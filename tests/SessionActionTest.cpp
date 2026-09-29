#include "sessionaction.h"
#include <cstdlib>
int main()
{
    const auto logout = sessionActionPrompt(QStringLiteral("logout"));
    if (logout.service() != QStringLiteral("org.kde.Shutdown")
        || logout.path() != QStringLiteral("/Shutdown")
        || logout.interface() != QStringLiteral("org.kde.Shutdown")
        || logout.member() != QStringLiteral("logout") || !logout.arguments().isEmpty()) return EXIT_FAILURE;
    const QString actions[]{QStringLiteral("restart"), QStringLiteral("shutdown")};
    const QString methods[]{QStringLiteral("promptReboot"), QStringLiteral("promptShutDown")};
    for (int i = 0; i < 2; ++i) {
        const auto message = sessionActionPrompt(actions[i]);
        if (message.service() != QStringLiteral("org.kde.LogoutPrompt")
            || message.path() != QStringLiteral("/LogoutPrompt")
            || message.interface() != QStringLiteral("org.kde.LogoutPrompt")
            || message.member() != methods[i] || !message.arguments().isEmpty()) return EXIT_FAILURE;
    }
    return sessionActionPrompt(QStringLiteral("invalid")).type() == QDBusMessage::InvalidMessage ? EXIT_SUCCESS : EXIT_FAILURE;
}
