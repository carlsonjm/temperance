/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDateTime>
#include <QObject>
#include <optional>

class QDBusServiceWatcher;
class QTimer;

// Control Center's Keep awake: while it is on, the screen does not dim or
// lock and the computer does not sleep on its own. It asks Plasma's power
// management the way Plasma's own Power and Battery page does, so closing the
// lid, the power button and a Sleep chosen by hand still act. What it holds is
// tied to the panel's own connection, so it ends at sign-out and is never
// carried into the next session. It can be given a time to end by itself.
//
// The shell keeps one, however many Status Bars it shows, and other programs
// of the suite reach it on the session bus (openDoor): Search's quick action
// turns it on, for a time or until sign-out, and off.
class KeepAwake : public QObject
{
    Q_OBJECT
    // On as the person last asked; it follows a tap at once and falls back
    // only if Plasma refuses.
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    // Whether Plasma's power management is there to ask.
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    // When it ends by itself; not valid while it lasts until sign-out, and
    // while off. `timed` says which.
    Q_PROPERTY(QDateTime until READ until NOTIFY activeChanged)
    Q_PROPERTY(bool timed READ timed NOTIFY activeChanged)

public:
    static constexpr char SleepService[] = "org.freedesktop.PowerManagement.Inhibit";
    static constexpr char SleepPath[] = "/org/freedesktop/PowerManagement/Inhibit";
    static constexpr char ScreenService[] = "org.freedesktop.ScreenSaver";
    static constexpr char ScreenPath[] = "/ScreenSaver";
    // The door other programs use: Active() → b, Until() → s (ISO 8601, or
    // empty), Set(b active, s until) → b, and the signal Changed(b, s).
    static constexpr char DoorService[] = "co.goodinput.Temperance";
    static constexpr char DoorPath[] = "/KeepAwake";
    static constexpr char DoorInterface[] = "co.goodinput.Temperance.KeepAwake";

    explicit KeepAwake(QObject *parent = nullptr);
    ~KeepAwake() override;

    // Follows power management from here on.
    void start();
    // Offers it on the session bus. False when the name is taken.
    bool openDoor();

    bool active() const;
    bool available() const;
    QDateTime until() const;
    bool timed() const;

    Q_INVOKABLE void toggle();
    Q_INVOKABLE void setActive(bool active);
    // On until `until`, when it turns itself off; a time not ahead is
    // refused. Without a valid time it is on until sign-out.
    Q_INVOKABLE bool setActiveUntil(const QDateTime &until);

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
    void setUntil(const QDateTime &until);
    void timeUp();

    QDBusServiceWatcher *m_watcher = nullptr;
    QTimer *m_end = nullptr;
    QDateTime m_until;
    bool m_wanted = false;
    bool m_available = false;
    bool m_busy = false;
    std::optional<uint> m_sleepCookie;
    std::optional<uint> m_screenCookie;
};
