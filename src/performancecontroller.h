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

class ScreenRefresh;

// Performance profiles on machines with the z13ctl-plus helper, such as the
// ROG Flow Z13. A capability-gated extension: without the helper every
// property stays empty and Control Center leaves the entry out. The list is
// read when start() finds the helper, again whenever Control Center opens,
// and after a preset is applied; the helper is never run on a timer.
class PerformanceController : public QObject
{
    Q_OBJECT
    Q_MOC_INCLUDE("screenrefresh.h")
    Q_PROPERTY(QStringList presets READ presets NOTIFY presetsChanged)
    Q_PROPERTY(QString activePreset READ activePreset NOTIFY activePresetChanged)
    // What each preset holds (profile, energy use, power limits, fan curve),
    // the AC/battery assignment and the charge limit, read from the
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

    // Looks for the helper and, when it is there, reads the presets.
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

private:
    void readPresets(std::function<void()> done);
    void refreshState();
    void setBusy(bool busy);
    void sendRequests(QList<QJsonObject> requests, std::function<void(bool ok, const QJsonObject &reply)> done);

    QString m_helper;
    QStringList m_presets;
    QString m_activePreset;
    QVariantMap m_presetSettings;
    bool m_autoSwitch = false;
    QString m_acPreset;
    QString m_batteryPreset;
    int m_batteryLimit = 0;
    bool m_busy = false;
    ScreenRefresh *m_screenRefresh = nullptr;
};
