/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDeadlineTimer>
#include <QList>
#include <QObject>
#include <QString>
#include <functional>

#include <KScreen/Types>

// The built-in screen's refresh rate, chosen by hand from the Performance
// page. The profile in use normally sets the rate; a rate chosen here holds
// until the profile changes. The z13ctl-plus helper re-applies the profile
// when the machine wakes, which would undo the hold although the profile
// has not changed, so a rate lost just after waking is put back, and only
// once the screen is unlocked. Any other change of rate ends the hold.
class ScreenRefresh : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QList<int> rates READ rates NOTIFY changed)
    Q_PROPERTY(int rate READ rate NOTIFY changed)
    Q_PROPERTY(bool held READ held NOTIFY changed)

public:
    // recheckProfile reads the profile in use again, reports it through
    // setProfile, then calls the function it is given.
    ScreenRefresh(std::function<void(std::function<void()>)> recheckProfile, QObject *parent);

    QList<int> rates() const;
    int rate() const;
    bool held() const;

    Q_INVOKABLE void choose(int hz);
    void setProfile(const QString &profile);

Q_SIGNALS:
    void changed();

private:
    KScreen::OutputPtr panel() const;
    void readConfig();
    void apply(int hz);
    void restoreWhenUnlocked();

private Q_SLOTS:
    void onPrepareForSleep(bool sleeping);
    void onScreenSaverActiveChanged(bool active);

private:
    std::function<void(std::function<void()>)> m_recheckProfile;
    KScreen::ConfigPtr m_config;
    QList<int> m_rates;
    int m_rate = 0;
    int m_heldRate = 0;
    // The rate the profile set, known while nothing is held.
    int m_profileRate = 0;
    QString m_profile;
    QDeadlineTimer m_wakeWindow;
    bool m_restorePending = false;
};
