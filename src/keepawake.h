/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <optional>

class QDBusServiceWatcher;

// Control Center's Keep awake: while it is on, the screen does not dim or
// lock and the computer does not sleep on its own. It asks Plasma's power
// management the way Plasma's own Power and Battery page does, so closing the
// lid, the power button and a Sleep chosen by hand still act. What it holds is
// tied to the panel's own connection, so it ends at sign-out and is never
// carried into the next session.
class KeepAwake : public QObject
{
    Q_OBJECT
    // On as the person last asked; it follows a tap at once and falls back
    // only if Plasma refuses.
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    // Whether Plasma's power management is there to ask.
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    static constexpr char SleepService[] = "org.freedesktop.PowerManagement.Inhibit";
    static constexpr char SleepPath[] = "/org/freedesktop/PowerManagement/Inhibit";
    static constexpr char ScreenService[] = "org.freedesktop.ScreenSaver";
    static constexpr char ScreenPath[] = "/ScreenSaver";

    explicit KeepAwake(QObject *parent = nullptr);
    ~KeepAwake() override;

    // Follows power management from here on.
    void start();

    bool active() const;
    bool available() const;

    Q_INVOKABLE void toggle();
    Q_INVOKABLE void setActive(bool active);

Q_SIGNALS:
    void activeChanged();
    void availableChanged();
    // Plasma would not keep the computer awake; the switch is off again.
    void failed();

private:
    void settle();
    void hold();
    void release();
    void setAvailable(bool available);
    void lost();

    QDBusServiceWatcher *m_watcher = nullptr;
    bool m_wanted = false;
    bool m_available = false;
    bool m_busy = false;
    std::optional<uint> m_sleepCookie;
    std::optional<uint> m_screenCookie;
};
