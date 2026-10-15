/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "performancecontroller.h"

#include "fancurve.h"
#include "screenrefresh.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <memory>

using namespace Qt::StringLiterals;

namespace
{
constexpr int helperTimeout = 5000;
}

PerformanceController::PerformanceController(QObject *parent)
    : QObject(parent)
{
}

void PerformanceController::start()
{
    // A successful helper discovery unlocks the control without creating a
    // separate build or exposing device-specific setup to everyone else.
    // Only the system's own copy is run, as its package installs it, never
    // one that comes first on the panel's PATH.
    m_helper = QStandardPaths::findExecutable(u"z13ctl-plus"_s, {u"/usr/bin"_s, u"/usr/local/bin"_s});
    if (!m_helper.isEmpty()) {
        m_screenRefresh = new ScreenRefresh([this](std::function<void()> done) { readPresets(std::move(done)); }, this);
        Q_EMIT screenRefreshChanged();
    }
    refresh();
}

QStringList PerformanceController::presets() const
{
    return m_presets;
}

QString PerformanceController::activePreset() const
{
    return m_activePreset;
}

ScreenRefresh *PerformanceController::screenRefresh() const
{
    return m_screenRefresh;
}

void PerformanceController::refresh()
{
    readPresets({});
}

// Runs the helper with a time limit: one that hangs is stopped after five
// seconds, and one that cannot start ends at once, so whatever waits on it,
// such as the refresh rate's check after waking, always hears back.
void PerformanceController::runHelper(const QStringList &arguments,
                                      std::function<void(QProcess *process, bool ok)> finished)
{
    auto *process = new QProcess(this);
    auto ended = std::make_shared<bool>(false);
    auto end = [process, ended, finished](bool ok) {
        if (*ended) {
            return;
        }
        *ended = true;
        finished(process, ok);
        process->deleteLater();
    };
    connect(process, &QProcess::finished, this, [end](int exitCode, QProcess::ExitStatus status) {
        end(status == QProcess::NormalExit && exitCode == 0);
    });
    connect(process, &QProcess::errorOccurred, this, [end](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            end(false);
        }
    });
    QTimer::singleShot(helperTimeout, process, [process]() {
        process->kill();
    });
    process->start(m_helper, arguments);
}

void PerformanceController::readPresets(std::function<void()> done)
{
    if (m_helper.isEmpty()) {
        return;
    }
    runHelper({u"preset"_s, u"list"_s}, [this, done](QProcess *process, bool ok) {
        if (ok) {
            QStringList presets;
            QString active;
            const QString output = QString::fromUtf8(process->readAllStandardOutput());
            const auto lines = output.split(u'\n', Qt::SkipEmptyParts);
            for (const QString &rawLine : lines) {
                const QString line = rawLine.trimmed();
                const qsizetype colon = line.indexOf(u':');
                if (colon <= 0 || line.startsWith(u"Auto:")) {
                    continue;
                }
                QString name = line.left(colon).trimmed();
                const QString activeSuffix = u" (active)"_s;
                if (name.endsWith(activeSuffix)) {
                    name.chop(activeSuffix.size());
                    active = name;
                }
                if (!name.isEmpty() && !presets.contains(name)) {
                    presets.append(name);
                }
            }
            if (presets != m_presets) {
                m_presets = presets;
                Q_EMIT presetsChanged();
            }
            if (active != m_activePreset) {
                m_activePreset = active;
                Q_EMIT activePresetChanged();
            }
            if (m_screenRefresh && !active.isEmpty()) {
                m_screenRefresh->setProfile(active);
            }
        }
        if (done) {
            done();
        }
    });
    refreshState();
}

QVariantMap PerformanceController::presetSettings() const
{
    return m_presetSettings;
}

bool PerformanceController::autoSwitch() const
{
    return m_autoSwitch;
}

QString PerformanceController::acPreset() const
{
    return m_acPreset;
}

QString PerformanceController::batteryPreset() const
{
    return m_batteryPreset;
}

int PerformanceController::batteryLimit() const
{
    return m_batteryLimit;
}

bool PerformanceController::busy() const
{
    return m_busy;
}

void PerformanceController::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    Q_EMIT busyChanged();
}

// Sends each request to the z13ctl-plus daemon in turn, one connection per
// request as its own client does, and stops at the first refusal.
void PerformanceController::sendRequests(QList<QJsonObject> requests,
                                         std::function<void(bool ok, const QJsonObject &reply)> done)
{
    if (requests.isEmpty()) {
        done(true, {});
        return;
    }
    const QJsonObject request = requests.takeFirst();
    const QString path = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
        + u"/z13ctl-plus/z13ctl-plus.sock"_s;
    auto *socket = new QLocalSocket(this);
    auto finished = std::make_shared<bool>(false);
    auto finish = [this, socket, finished, requests, done](bool ok, const QJsonObject &reply) {
        if (*finished) {
            return;
        }
        *finished = true;
        socket->disconnect(this);
        socket->deleteLater();
        if (!ok || requests.isEmpty()) {
            done(ok, reply);
        } else {
            sendRequests(requests, done);
        }
    };
    connect(socket, &QLocalSocket::connected, this, [socket, request]() {
        socket->write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    });
    connect(socket, &QLocalSocket::readyRead, this, [socket, finish]() {
        if (!socket->canReadLine()) {
            return;
        }
        const QJsonObject reply = QJsonDocument::fromJson(socket->readLine()).object();
        finish(reply.value(u"ok"_s).toBool(), reply);
    });
    connect(socket, &QLocalSocket::errorOccurred, this, [finish](QLocalSocket::LocalSocketError) {
        finish(false, {});
    });
    QTimer::singleShot(5000, socket, [finish]() {
        finish(false, {});
    });
    socket->connectToServer(path);
}

void PerformanceController::refreshState()
{
    if (m_helper.isEmpty() || m_busy) {
        return;
    }
    sendRequests({QJsonObject{{u"cmd"_s, u"get-state"_s}}}, [this](bool ok, const QJsonObject &reply) {
        if (!ok) {
            return;
        }
        const QJsonObject state = reply.value(u"state"_s).toObject();
        // Each preset as the Performance page edits it. A preset without
        // power limits or a fan curve leaves them to the profile's firmware,
        // so those keys are left out rather than filled with live values.
        QVariantMap settings;
        const QJsonObject presets = state.value(u"presets"_s).toObject();
        for (auto it = presets.begin(); it != presets.end(); ++it) {
            const QJsonObject preset = it.value().toObject();
            QVariantMap entry;
            entry.insert(u"profile"_s, preset.value(u"profile"_s).toString());
            entry.insert(u"epp"_s, preset.value(u"cpu_power"_s).toObject().value(u"epp"_s).toString());
            const QJsonObject tdp = preset.value(u"tdp"_s).toObject();
            if (!tdp.isEmpty()) {
                entry.insert(u"pl1"_s, tdp.value(u"pl1_spl"_s).toInt());
                entry.insert(u"pl2"_s, tdp.value(u"pl2_sppt"_s).toInt());
                entry.insert(u"pl3"_s, tdp.value(u"fppt"_s).toInt());
            }
            const QJsonArray points = preset.value(u"fan_curve"_s).toObject().value(u"points"_s).toArray();
            if (points.size() == 8) {
                QStringList curve;
                for (qsizetype i = 0; i < points.size(); ++i) {
                    const QJsonObject point = points.at(i).toObject();
                    curve.append(u"%1:%2"_s.arg(point.value(u"temp"_s).toInt()).arg(point.value(u"pwm"_s).toInt()));
                }
                entry.insert(u"fanCurve"_s, curve.join(u','));
            }
            settings.insert(it.key(), entry);
        }
        const QJsonObject policy = state.value(u"power_policy"_s).toObject();
        // The live threshold first; the daemon's own record when the
        // battery could not be read.
        int batteryLimit = state.value(u"battery_detail"_s).toObject().value(u"threshold_pct"_s).toInt();
        if (batteryLimit <= 0) {
            batteryLimit = state.value(u"battery_limit"_s).toInt();
        }
        m_presetSettings = settings;
        m_autoSwitch = policy.value(u"enabled"_s).toBool();
        m_acPreset = policy.value(u"ac_preset"_s).toString();
        m_batteryPreset = policy.value(u"battery_preset"_s).toString();
        m_batteryLimit = batteryLimit;
        Q_EMIT settingsChanged();
    });
}

// The daemon can only save what is live, so a preset is changed by applying
// it, setting only what the Performance page changed, saving it under the
// same name and applying the preset that was active before. On any refusal
// the earlier preset is applied again, so the machine never stays on a
// half-edited preset. Nothing else saves a preset.
void PerformanceController::savePreset(const QString &name, const QVariantMap &settings)
{
    static const QStringList profiles{u"quiet"_s, u"balanced"_s, u"performance"_s};
    static const QStringList epps{u"power"_s, u"balance_power"_s, u"balance_performance"_s, u"performance"_s};
    if (m_helper.isEmpty() || m_busy || !m_presets.contains(name)) {
        Q_EMIT presetSaved(name, false, QString());
        return;
    }

    QList<QJsonObject> changes;
    if (settings.contains(u"profile"_s)) {
        const QString profile = settings.value(u"profile"_s).toString();
        if (!profiles.contains(profile)) {
            Q_EMIT presetSaved(name, false, QString());
            return;
        }
        changes.append(QJsonObject{{u"cmd"_s, u"profile"_s}, {u"set"_s, profile}});
    }
    if (settings.value(u"tdpReset"_s).toBool()) {
        changes.append(QJsonObject{{u"cmd"_s, u"tdp-reset"_s}});
    } else if (settings.contains(u"pl1"_s)) {
        const int pl1 = settings.value(u"pl1"_s).toInt();
        const int pl2 = settings.value(u"pl2"_s).toInt();
        const int pl3 = settings.value(u"pl3"_s).toInt();
        // Sustained power stays where the helper allows it without force;
        // the boosts within the hardware's maximum, as the helper checks.
        if (pl1 < 5 || pl1 > 75 || pl2 < 5 || pl2 > 93 || pl3 < 5 || pl3 > 93) {
            Q_EMIT presetSaved(name, false, QString());
            return;
        }
        changes.append(QJsonObject{{u"cmd"_s, u"tdp"_s},
                                   {u"pl1"_s, QString::number(pl1)},
                                   {u"pl2"_s, QString::number(pl2)},
                                   {u"pl3"_s, QString::number(pl3)}});
    }
    if (settings.value(u"fanReset"_s).toBool()) {
        changes.append(QJsonObject{{u"cmd"_s, u"fancurve-reset"_s}});
    } else if (settings.contains(u"fanCurve"_s)) {
        // Eight temp:pwm points that keep the fans turning when hot (fancurve.h).
        const QString curve = settings.value(u"fanCurve"_s).toString();
        if (!fanCurveAllowed(curve)) {
            Q_EMIT presetSaved(name, false, QString());
            return;
        }
        changes.append(QJsonObject{{u"cmd"_s, u"fancurve"_s}, {u"set"_s, curve}});
    }
    if (settings.contains(u"epp"_s)) {
        const QString epp = settings.value(u"epp"_s).toString();
        if (!epps.contains(epp)) {
            Q_EMIT presetSaved(name, false, QString());
            return;
        }
        changes.append(QJsonObject{{u"cmd"_s, u"cpu-epp"_s}, {u"set"_s, epp}});
    }
    if (changes.isEmpty()) {
        Q_EMIT presetSaved(name, true, QString());
        return;
    }

    // With no preset active there is nothing to return to, so the edited
    // preset stays applied.
    const QString restore = m_activePreset.isEmpty() ? name : m_activePreset;
    const QJsonObject restoreRequest{{u"cmd"_s, u"preset-apply"_s}, {u"name"_s, restore}};
    QList<QJsonObject> requests{QJsonObject{{u"cmd"_s, u"preset-apply"_s}, {u"name"_s, name}}};
    requests.append(changes);
    requests.append(QJsonObject{{u"cmd"_s, u"preset-save"_s}, {u"name"_s, name}});
    setBusy(true);
    sendRequests(requests, [this, name, restoreRequest](bool saved, const QJsonObject &reply) {
        const QString detail = saved ? QString() : reply.value(u"error"_s).toString();
        sendRequests({restoreRequest}, [this, name, saved, detail](bool, const QJsonObject &) {
            setBusy(false);
            Q_EMIT presetSaved(name, saved, detail);
            refresh();
        });
    });
}

void PerformanceController::setBatteryLimit(int percent)
{
    // The helper accepts 40 to 100 percent.
    if (m_helper.isEmpty() || m_busy || percent < 40 || percent > 100) {
        return;
    }
    setBusy(true);
    sendRequests({QJsonObject{{u"cmd"_s, u"batterylimit"_s}, {u"set"_s, QString::number(percent)}}},
                            [this](bool, const QJsonObject &) {
                                setBusy(false);
                                refreshState();
                            });
}

void PerformanceController::setPowerPolicy(bool enabled, const QString &acPreset, const QString &batteryPreset)
{
    if (m_helper.isEmpty() || m_busy
        || (!acPreset.isEmpty() && !m_presets.contains(acPreset))
        || (!batteryPreset.isEmpty() && !m_presets.contains(batteryPreset))) {
        return;
    }
    setBusy(true);
    sendRequests({QJsonObject{{u"cmd"_s, u"power-policy-set"_s},
                              {u"enabled"_s, enabled},
                              {u"ac_preset"_s, acPreset},
                              {u"battery_preset"_s, batteryPreset}}},
                            [this](bool, const QJsonObject &) {
                                setBusy(false);
                                refresh();
                            });
}

void PerformanceController::applyPreset(const QString &name)
{
    if (m_helper.isEmpty() || m_busy || name.isEmpty()
        || (!m_presets.isEmpty() && !m_presets.contains(name))) {
        return;
    }
    runHelper({u"preset"_s, u"apply"_s, name}, [this](QProcess *, bool) {
        QTimer::singleShot(300, this, &PerformanceController::refresh);
    });
}
