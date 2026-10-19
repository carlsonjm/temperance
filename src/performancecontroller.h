/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <functional>

class QJsonObject;
class ScreenRefresh;

// Performance profiles on machines with the z13ctl-plus helper, such as the
// ROG Flow Z13. A capability-gated extension: while the helper's daemon does
// not answer on its socket every property stays empty and Control Center
// leaves the entry out. Everything is read from the daemon, never by running
// a copy of the helper, so the page and the daemon cannot disagree. The list
// is read at start, again whenever Control Center opens, after a preset is
// applied and when the power cable comes or goes; the daemon is never asked
// on a timer.
class PerformanceController : public QObject
{
    Q_OBJECT
    Q_MOC_INCLUDE("screenrefresh.h")
    Q_PROPERTY(QStringList presets READ presets NOTIFY presetsChanged)
    Q_PROPERTY(QString activePreset READ activePreset NOTIFY activePresetChanged)
    // What each preset holds (profile, energy use, power limits, fan curve,
    // and the charge limit Temperance keeps for it), the AC/battery
    // assignment and the live charge limit, read from the
    // z13ctl-plus daemon so the Performance page edits a preset from what it
    // saved rather than from whatever happens to be live.
    Q_PROPERTY(QVariantMap presetSettings READ presetSettings NOTIFY settingsChanged)
    Q_PROPERTY(bool autoSwitch READ autoSwitch NOTIFY settingsChanged)
    Q_PROPERTY(QString acPreset READ acPreset NOTIFY settingsChanged)
    Q_PROPERTY(QString batteryPreset READ batteryPreset NOTIFY settingsChanged)
    Q_PROPERTY(int batteryLimit READ batteryLimit NOTIFY settingsChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // The built-in screen's refresh rate, alongside the profiles; null
    // without the helper.
    Q_PROPERTY(ScreenRefresh *screenRefresh READ screenRefresh NOTIFY screenRefreshChanged)

public:
    explicit PerformanceController(QObject *parent = nullptr);

    // Asks the helper's daemon for the presets.
    void start();

    QStringList presets() const;
    QString activePreset() const;
    QVariantMap presetSettings() const;
    bool autoSwitch() const;
    QString acPreset() const;
    QString batteryPreset() const;
    int batteryLimit() const;
    bool busy() const;
    ScreenRefresh *screenRefresh() const;

    Q_INVOKABLE void applyPreset(const QString &name);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void savePreset(const QString &name, const QVariantMap &settings);
    Q_INVOKABLE void createPreset(const QString &name, const QString &source, const QVariantMap &settings);
    // Sets the live charge limit and keeps it as the preset in use's own.
    Q_INVOKABLE void setBatteryLimit(int percent);
    Q_INVOKABLE void setPowerPolicy(bool enabled, const QString &acPreset, const QString &batteryPreset);

Q_SIGNALS:
    void presetsChanged();
    void activePresetChanged();
    void settingsChanged();
    void busyChanged();
    void screenRefreshChanged();
    // A preset save has ended; detail is the helper's reason when it gave one.
    void presetSaved(const QString &name, bool ok, const QString &detail);

private Q_SLOTS:
    void powerSourceChanged();

private:
    int presetChargeLimit(const QString &name) const;
    void storeChargeLimit(const QString &name, int percent);
    void followChargeLimit();
    void readPresets(std::function<void()> done);
    void readSettings(const QJsonObject &state);
    void setBusy(bool busy);
    void writePreset(const QString &source, const QString &name, const QVariantMap &settings);
    void sendRequests(QList<QJsonObject> requests, std::function<void(bool ok, const QJsonObject &reply)> done);

    bool m_available = false;
    QStringList m_presets;
    QString m_activePreset;
    QVariantMap m_presetSettings;
    bool m_autoSwitch = false;
    QString m_acPreset;
    QString m_batteryPreset;
    int m_batteryLimit = 0;
    // The limit last sent to follow the preset in use, so a refused one is
    // not sent again until the live limit changes.
    int m_chargeLimitSent = 0;
    bool m_busy = false;
    ScreenRefresh *m_screenRefresh = nullptr;
};
