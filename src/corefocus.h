/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>

// What Split Rock's Core hears from Status about Do Not Disturb: when it
// ends, how many notifications came in meanwhile, so Core can say it once on
// the island; and Core asking for those notifications to be shown. Without
// Split Rock nothing is sent, and nothing asks.
class CoreFocus : public QObject
{
    Q_OBJECT

public:
    static constexpr char Service[] = "io.github.carlsonjm.splitrock.Core";
    static constexpr char Path[] = "/Core";
    static constexpr char Interface[] = "io.github.carlsonjm.SplitRock.Core";

    explicit CoreFocus(QObject *parent = nullptr);

    // Listens for Core's request on the session bus.
    void start();

    // Do Not Disturb ended with this many notifications come in. Core is
    // told only while it runs; it is never started for this.
    Q_INVOKABLE void ended(int notifications);
    // The same, ended inside the notification history, where the person
    // already sees what came in: Core puts nothing on the island.
    Q_INVOKABLE void endedInView(int notifications);
    // The history was opened, so what came in is seen: Core takes its island
    // about it down.
    Q_INVOKABLE void seen();

Q_SIGNALS:
    // Core asked to see the notifications that came in.
    void openRequested();

private Q_SLOTS:
    void requested();

private:
    void send(const QString &method, const QVariantList &arguments = {});
    bool m_started = false;
};
