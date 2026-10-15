/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "clockextent.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

ClockExtent::ClockExtent(QObject *parent)
    : QObject(parent)
{
    auto bus = QDBusConnection::sessionBus();
    bus.connect(QString::fromLatin1(service), QString::fromLatin1(path), QString::fromLatin1(interface),
                u"clockExtentChanged"_s, this, SLOT(onExtentChanged(QString)));
    // A host that comes up after the applet is asked once when it does, and
    // one that goes away leaves the width to the panel again.
    m_watcher = new QDBusServiceWatcher(QString::fromLatin1(service), bus,
                                        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, [this] {
        if (m_asked)
            ask();
    });
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
        ++m_request;
        setAvailableWidth(0);
    });
}

void ClockExtent::setOutput(const QString &outputName)
{
    if (m_asked && outputName == m_output)
        return;
    m_output = outputName;
    m_asked = true;
    ask();
}

int ClockExtent::availableWidth() const
{
    return m_availableWidth;
}

void ClockExtent::onExtentChanged(const QString &outputName)
{
    if (m_asked && (outputName.isEmpty() || outputName == m_output))
        ask();
}

void ClockExtent::ask()
{
    const quint64 request = ++m_request;
    auto message = QDBusMessage::createMethodCall(QString::fromLatin1(service), QString::fromLatin1(path),
                                                  QString::fromLatin1(interface), u"clockExtent"_s);
    message << m_output;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, request](QDBusPendingCallWatcher *call) {
        call->deleteLater();
        // A later question, or the host leaving, supersedes this answer.
        if (request != m_request)
            return;
        const QDBusPendingReply<QString> reply = *call;
        setAvailableWidth(reply.isError() ? 0 : parseAvailableWidth(reply.value()));
    });
}

int ClockExtent::parseAvailableWidth(const QString &reply)
{
    const QJsonObject extent = QJsonDocument::fromJson(reply.toUtf8()).object();
    if (extent.value("schema"_L1).toString() != "co.goodinput.shuffle.clock-extent"_L1
        || !extent.value("version"_L1).isDouble() || extent.value("version"_L1).toDouble() != 1
        || extent.value("presenting"_L1) != QJsonValue(true))
        return 0;
    const QJsonValue right = extent.value("available"_L1).toObject().value("right"_L1);
    return right.isDouble() && right.toDouble() > 0 ? int(right.toDouble()) : 0;
}

void ClockExtent::setAvailableWidth(int width)
{
    if (width == m_availableWidth)
        return;
    m_availableWidth = width;
    Q_EMIT availableWidthChanged();
}
