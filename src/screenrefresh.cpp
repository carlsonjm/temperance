/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "screenrefresh.h"

#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusMessage>

#include <KScreen/Config>
#include <KScreen/ConfigMonitor>
#include <KScreen/GetConfigOperation>
#include <KScreen/Mode>
#include <KScreen/Output>
#include <KScreen/SetConfigOperation>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace
{
// How long after waking a lost rate counts as the helper re-applying the
// profile rather than someone choosing another rate.
constexpr int wakeWindowMs = 20000;
const QString screenSaverService = u"org.freedesktop.ScreenSaver"_s;
const QString screenSaverPath = u"/ScreenSaver"_s;
}

ScreenRefresh::ScreenRefresh(std::function<void(std::function<void()>)> recheckProfile, QObject *parent)
    : QObject(parent)
    , m_recheckProfile(std::move(recheckProfile))
{
    auto *operation = new KScreen::GetConfigOperation();
    connect(operation, &KScreen::ConfigOperation::finished, this, [this](KScreen::ConfigOperation *op) {
        if (op->hasError()) {
            return;
        }
        m_config = qobject_cast<KScreen::GetConfigOperation *>(op)->config();
        KScreen::ConfigMonitor::instance()->addConfig(m_config);
        connect(KScreen::ConfigMonitor::instance(), &KScreen::ConfigMonitor::configurationChanged, this, &ScreenRefresh::readConfig);
        readConfig();
    });

    QDBusConnection::systemBus().connect(u"org.freedesktop.login1"_s,
                                         u"/org/freedesktop/login1"_s,
                                         u"org.freedesktop.login1.Manager"_s,
                                         u"PrepareForSleep"_s,
                                         this,
                                         SLOT(onPrepareForSleep(bool)));
    QDBusConnection::sessionBus().connect(screenSaverService,
                                          screenSaverPath,
                                          screenSaverService,
                                          u"ActiveChanged"_s,
                                          this,
                                          SLOT(onScreenSaverActiveChanged(bool)));
}

QList<int> ScreenRefresh::rates() const
{
    return m_rates;
}

int ScreenRefresh::rate() const
{
    return m_rate;
}

bool ScreenRefresh::held() const
{
    return m_heldRate > 0;
}

// The machine's own screen, or failing that the only screen in use.
KScreen::OutputPtr ScreenRefresh::panel() const
{
    if (!m_config) {
        return {};
    }
    KScreen::OutputPtr only;
    int enabled = 0;
    for (const KScreen::OutputPtr &output : m_config->outputs()) {
        if (!output->isConnected() || !output->isEnabled()) {
            continue;
        }
        if (output->type() == KScreen::Output::Panel) {
            return output;
        }
        only = output;
        ++enabled;
    }
    return enabled == 1 ? only : KScreen::OutputPtr();
}

void ScreenRefresh::readConfig()
{
    QList<int> rates;
    int rate = 0;
    if (const KScreen::OutputPtr output = panel()) {
        if (const KScreen::ModePtr current = output->currentMode()) {
            rate = int(std::lround(current->refreshRate()));
            for (const KScreen::ModePtr &mode : output->modes()) {
                const int hz = int(std::lround(mode->refreshRate()));
                if (mode->size() == current->size() && hz > 0 && !rates.contains(hz)) {
                    rates.append(hz);
                }
            }
            std::sort(rates.begin(), rates.end());
        }
    }
    const bool lost = m_heldRate > 0 && rate > 0 && rate != m_heldRate && rate != m_rate;
    if (m_heldRate == 0 && rate > 0) {
        m_profileRate = rate;
    }
    if (rates != m_rates || rate != m_rate) {
        m_rates = rates;
        m_rate = rate;
        Q_EMIT changed();
    }
    if (!lost) {
        return;
    }
    if (m_wakeWindow.hasExpired()) {
        // Someone else chose a rate, or a new profile set its own.
        m_heldRate = 0;
        m_profileRate = rate;
        m_restorePending = false;
        Q_EMIT changed();
        return;
    }
    // Just after waking: put the rate back only if the profile is the same,
    // since a new profile ends the hold.
    m_recheckProfile([this]() {
        if (m_heldRate > 0 && m_rate != m_heldRate) {
            restoreWhenUnlocked();
        }
    });
}

void ScreenRefresh::choose(int hz)
{
    if (!m_rates.contains(hz)) {
        return;
    }
    // Back to the profile's own rate is no longer a manual choice.
    m_heldRate = hz == m_profileRate ? 0 : hz;
    m_restorePending = false;
    Q_EMIT changed();
    apply(hz);
}

void ScreenRefresh::setProfile(const QString &profile)
{
    if (profile == m_profile) {
        return;
    }
    const bool hadProfile = !m_profile.isEmpty();
    m_profile = profile;
    if (hadProfile && m_heldRate > 0) {
        m_heldRate = 0;
        m_profileRate = m_rate;
        m_restorePending = false;
        Q_EMIT changed();
    }
}

void ScreenRefresh::apply(int hz)
{
    const KScreen::OutputPtr live = panel();
    if (!live || !live->currentMode()) {
        return;
    }
    const QSize size = live->currentMode()->size();
    QString modeId;
    double nearest = 0;
    for (const KScreen::ModePtr &mode : live->modes()) {
        const double distance = std::abs(mode->refreshRate() - hz);
        if (mode->size() == size && (modeId.isEmpty() || distance < nearest)) {
            modeId = mode->id();
            nearest = distance;
        }
    }
    if (modeId.isEmpty() || modeId == live->currentModeId()) {
        return;
    }
    const KScreen::ConfigPtr config = m_config->clone();
    const KScreen::OutputPtr output = config->output(live->id());
    if (!output) {
        return;
    }
    output->setCurrentModeId(modeId);
    new KScreen::SetConfigOperation(config);
}

// Never while the lock screen is up: changing the screen under it is the
// one thing known to upset it.
void ScreenRefresh::restoreWhenUnlocked()
{
    m_restorePending = true;
    const QDBusMessage message = QDBusMessage::createMethodCall(screenSaverService, screenSaverPath, screenSaverService, u"GetActive"_s);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
        const QDBusPendingReply<bool> reply = *call;
        call->deleteLater();
        if (!reply.isError() && !reply.value()) {
            onScreenSaverActiveChanged(false);
        }
    });
}

void ScreenRefresh::onPrepareForSleep(bool sleeping)
{
    if (!sleeping) {
        m_wakeWindow.setRemainingTime(wakeWindowMs);
    }
}

void ScreenRefresh::onScreenSaverActiveChanged(bool active)
{
    if (active || !m_restorePending) {
        return;
    }
    m_restorePending = false;
    if (m_heldRate > 0 && m_rate != m_heldRate) {
        apply(m_heldRate);
    }
}
