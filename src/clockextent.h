/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QString>

class QDBusServiceWatcher;

// The room a host leaves beside a clock it paints itself, read from the
// host's optional session-bus interface (docs/HOST-INTERFACES.md). The host
// pushes changes; nothing here asks on a timer. Without the host, or with a
// reply that is not understood, the width is 0 and the panel's own
// neighbours decide.
class ClockExtent : public QObject
{
    Q_OBJECT

public:
    static constexpr auto service = "co.goodinput.ContextBar";
    static constexpr auto path = "/ContextBar";
    static constexpr auto interface = "co.goodinput.ContextBar";

    explicit ClockExtent(QObject *parent = nullptr);

    // The output the applet's window is on; a change asks again.
    void setOutput(const QString &outputName);
    // The width from the clock's edge to the output's far edge, or 0.
    int availableWidth() const;

    // The width a reply carries, or 0 when it is not one this reads.
    static int parseAvailableWidth(const QString &reply);

Q_SIGNALS:
    void availableWidthChanged();

private Q_SLOTS:
    void onExtentChanged(const QString &outputName);

private:
    void ask();
    void setAvailableWidth(int width);

    QString m_output;
    bool m_asked = false;
    int m_availableWidth = 0;
    quint64 m_request = 0;
    QDBusServiceWatcher *m_watcher = nullptr;
};
