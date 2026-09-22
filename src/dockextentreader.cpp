/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "dockextentreader.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>

namespace
{
constexpr auto kService = "studio.warbler.BottomSurface";
constexpr auto kPath = "/BottomSurface";
constexpr auto kInterface = "studio.warbler.BottomSurface";
constexpr auto kSchema = "studio.warbler.shuffle.dock-extent";
}

DockExtentReader::DockExtentReader(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    auto *watcher = new QDBusServiceWatcher(QString::fromLatin1(kService), bus,
                                            QDBusServiceWatcher::WatchForOwnerChange,
                                            this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this,
            [this](const QString &) { onServiceOwned(); });
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this,
            [this](const QString &) { onServiceLost(); });

    // The signal is what this acts on; it never samples on a timer.
    bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                QString::fromLatin1(kInterface),
                QStringLiteral("dockExtentChanged"), this,
                SLOT(refresh(QString)));

    if (bus.interface()
        && bus.interface()
               ->isServiceRegistered(QString::fromLatin1(kService))
               .value()) {
        onServiceOwned();
    }
}

void DockExtentReader::onServiceOwned()
{
    m_available = true;
    const auto screens = QGuiApplication::screens();
    for (const QScreen *screen : screens) {
        if (screen) {
            refresh(screen->name());
        }
    }
}

void DockExtentReader::onServiceLost()
{
    if (!m_available && m_extents.isEmpty()) {
        return;
    }
    // Nothing published means compose as on a plain panel, not compose badly
    // on the last numbers seen.
    m_available = false;
    m_extents.clear();
    Q_EMIT extentChanged();
}

void DockExtentReader::refresh(const QString &outputName)
{
    if (!m_available || outputName.isEmpty()) {
        return;
    }

    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kService), QString::fromLatin1(kPath),
        QString::fromLatin1(kInterface), QStringLiteral("dockExtent"));
    call << outputName;

    auto *pending = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(call), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this,
            [this, outputName](QDBusPendingCallWatcher *self) {
                self->deleteLater();
                const QDBusPendingReply<QString> reply = *self;
                if (reply.isError()) {
                    return;
                }

                const QJsonObject payload =
                    QJsonDocument::fromJson(reply.value().toUtf8()).object();

                // An unknown major version is refused rather than guessed at.
                if (payload.value(QStringLiteral("schema")).toString()
                        != QLatin1String(kSchema)
                    || payload.value(QStringLiteral("version")).toInt() != 1) {
                    return;
                }

                const bool presenting =
                    payload.value(QStringLiteral("presenting")).toBool();
                const QJsonObject dock =
                    payload.value(QStringLiteral("dock")).toObject();

                if (!presenting) {
                    // A surface that is not presenting this output reserves
                    // nothing on it, which is not the same as no surface.
                    if (m_extents.remove(outputName) > 0) {
                        Q_EMIT extentChanged();
                    }
                    return;
                }

                const Extent extent{
                    dock.value(QStringLiteral("left")).toDouble(),
                    dock.value(QStringLiteral("right")).toDouble()};

                m_extents.insert(outputName, extent);
                Q_EMIT extentChanged();
            });
}

std::optional<qreal> DockExtentReader::rightEdgeFor(const QString &outputName) const
{
    const auto found = m_extents.constFind(outputName);
    if (found == m_extents.constEnd()) {
        return std::nullopt;
    }
    return found->right;
}

std::optional<qreal> DockExtentReader::leftEdgeFor(const QString &outputName) const
{
    const auto found = m_extents.constFind(outputName);
    if (found == m_extents.constEnd()) {
        return std::nullopt;
    }
    return found->left;
}
