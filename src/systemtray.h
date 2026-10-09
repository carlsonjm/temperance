/*
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractItemModel>
#include <QJsonObject>
#include <QVariantMap>
#include <functional>
#include <QPointer>
#include <QList>

#include <KConfigWatcher>

#include <Plasma/Containment>

#include "calendarfeeds.h"

class QQuickItem;

namespace Plasma
{
}
class PlasmoidRegistry;
class PlasmoidModel;
class SystemTraySettings;
class StatusNotifierModel;
class SystemTrayModel;
class SortedSystemTrayModel;
class KJob;
class ScreenRefresh;

namespace PulseAudioQt
{
class Sink;
}


class SystemTray : public Plasma::Containment
{
    Q_OBJECT
    Q_MOC_INCLUDE("screenrefresh.h")
    Q_PROPERTY(QAbstractItemModel *systemTrayModel READ sortedSystemTrayModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *configSystemTrayModel READ configSystemTrayModel CONSTANT)
    Q_PROPERTY(QStringList performancePresets READ performancePresets NOTIFY performancePresetsChanged)
    Q_PROPERTY(QString activePerformancePreset READ activePerformancePreset NOTIFY activePerformancePresetChanged)
    // What each preset holds (profile, energy use, power limits, fan curve),
    // the AC/battery assignment and the charge limit, read from the
    // z13ctl-plus daemon so the Performance page edits a preset from what it
    // saved rather than from whatever happens to be live.
    Q_PROPERTY(QVariantMap performancePresetSettings READ performancePresetSettings NOTIFY performanceStateChanged)
    Q_PROPERTY(bool performanceAutoSwitch READ performanceAutoSwitch NOTIFY performanceStateChanged)
    Q_PROPERTY(QString performanceAcPreset READ performanceAcPreset NOTIFY performanceStateChanged)
    Q_PROPERTY(QString performanceBatteryPreset READ performanceBatteryPreset NOTIFY performanceStateChanged)
    Q_PROPERTY(int performanceBatteryLimit READ performanceBatteryLimit NOTIFY performanceStateChanged)
    Q_PROPERTY(bool performanceBusy READ performanceBusy NOTIFY performanceBusyChanged)
    // The built-in screen's refresh rate, alongside the profiles; null
    // without the helper.
    Q_PROPERTY(ScreenRefresh *screenRefresh READ screenRefresh NOTIFY screenRefreshChanged)
    Q_PROPERTY(int volumePercent READ volumePercent NOTIFY volumeChanged)
    Q_PROPERTY(bool volumeMuted READ volumeMuted NOTIFY volumeChanged)
    Q_PROPERTY(bool volumeAvailable READ volumeAvailable NOTIFY volumeChanged)
    // The calendars linked in the settings, shared by the calendar card and
    // the settings page that shows each link's state.
    Q_PROPERTY(CalendarFeeds *calendarFeeds READ calendarFeeds CONSTANT)

public:
    SystemTray(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
    ~SystemTray() override;

    void init() override;

    void restoreContents(KConfigGroup &group) override;

    QAbstractItemModel *sortedSystemTrayModel();

    QAbstractItemModel *configSystemTrayModel();

    // Invocable utilities
    /**
     * Given an AppletInterface pointer, shows a proper context menu for it
     */
    Q_INVOKABLE void showPlasmoidMenu(QQuickItem *appletInterface, int x, int y);

    /**
     * Find out global coordinates for a popup given local MouseArea
     * coordinates
     */
    Q_INVOKABLE QPointF popupPosition(QQuickItem *visualParent, int x, int y);

    /**
     * Returns the horizontal panel space between this applet's fixed right
     * edge and the nearest non-spacer panel widget on its left.
     */
    Q_INVOKABLE int availablePanelWidth(QQuickItem *visualParent, int minimumWidth, int gap) const;
    Q_INVOKABLE void watchPanelGeometry(QQuickItem *visualParent);
    Q_INVOKABLE int availablePopupHeight(QQuickItem *visualParent) const;

    /**
     * @brief isSystemTrayApplet checks if applet is allowed in the System Tray
     * @param appletId also known as plugin Id
     * @return true if it is a system tray applet, otherwise false
     */
    Q_INVOKABLE bool isSystemTrayApplet(const QString &appletId);

    /**
     * Needed to preserve keyboard navigation
     */
    Q_INVOKABLE void stackItemBefore(QQuickItem *newItem, QQuickItem *beforeItem);

    Q_INVOKABLE void stackItemAfter(QQuickItem *newItem, QQuickItem *afterItem);

    Q_INVOKABLE void activate(const QString &service, QPoint pos, QQuickItem *statusNotifierIcon);

    Q_INVOKABLE void secondaryActivate(const QString &service, QPoint pos);

    Q_INVOKABLE void openContextMenu(const QString &service, QPoint pos, QQuickItem *statusNotifierIcon);

    Q_INVOKABLE void scroll(const QString &service, int delta, const QString &direction);
    Q_INVOKABLE void launchApplication(const QString &desktopName);
    Q_INVOKABLE void requestSessionAction(const QString &action);
    Q_INVOKABLE QString resolveApplicationIcon(const QString &applicationName,
                                               const QString &desktopEntry,
                                               const QString &fallbackIcon) const;

    QStringList performancePresets() const;
    QString activePerformancePreset() const;
    Q_INVOKABLE void applyPerformancePreset(const QString &name);
    Q_INVOKABLE void refreshPerformancePresets();
    QVariantMap performancePresetSettings() const;
    bool performanceAutoSwitch() const;
    QString performanceAcPreset() const;
    QString performanceBatteryPreset() const;
    int performanceBatteryLimit() const;
    bool performanceBusy() const;
    Q_INVOKABLE void savePerformancePreset(const QString &name, const QVariantMap &settings);
    Q_INVOKABLE void setPerformanceBatteryLimit(int percent);
    Q_INVOKABLE void setPerformancePowerPolicy(bool enabled, const QString &acPreset, const QString &batteryPreset);
    ScreenRefresh *screenRefresh() const;
    int volumePercent() const;
    bool volumeMuted() const;
    bool volumeAvailable() const;
    Q_INVOKABLE void setVolumePercent(int percent);
    CalendarFeeds *calendarFeeds() const;

Q_SIGNALS:
    void sessionActionFailed();
    void performancePresetsChanged();
    void activePerformancePresetChanged();
    void performanceStateChanged();
    void performanceBusyChanged();
    void screenRefreshChanged();
    // A preset save has ended; detail is the helper's reason when it gave one.
    void performancePresetSaved(const QString &name, bool ok, const QString &detail);
    void volumeChanged();
    void panelGeometryChanged();

private Q_SLOTS:
    // synchronizes with configuration and deletes not allowed applets
    void onEnabledAppletsChanged();
    // creates an applet *if not already existing*
    void startApplet(const QString &pluginId);
    // deletes/stops all instances of a given applet
    void stopApplet(const QString &pluginId);

private:
    QObject *m_sessionManagement = nullptr;
    void migrateFromSystrayContainer();
    SystemTrayModel *systemTrayModel();
    void initSettingsAndRegistry();
    void updateDefaultAudioSink(PulseAudioQt::Sink *sink);
    void watchGeometryItem(QQuickItem *item);

    KConfigWatcher::Ptr m_configWatcher;
    bool m_xwaylandClientsScale = true;

    QPointer<SystemTraySettings> m_settings;
    QPointer<PlasmoidRegistry> m_plasmoidRegistry;

    PlasmoidModel *m_plasmoidModel = nullptr;
    StatusNotifierModel *m_statusNotifierModel = nullptr;
    SystemTrayModel *m_systemTrayModel = nullptr;
    SortedSystemTrayModel *m_sortedSystemTrayModel = nullptr;
    SortedSystemTrayModel *m_configSystemTrayModel = nullptr;

    QHash<QString /*plugin id*/, int /*config group*/> m_configGroupIds;
    QStringList m_performancePresets;
    QString m_activePerformancePreset;
    QString m_performanceHelper;
    QVariantMap m_performancePresetSettings;
    bool m_performanceAutoSwitch = false;
    QString m_performanceAcPreset;
    QString m_performanceBatteryPreset;
    int m_performanceBatteryLimit = 0;
    bool m_performanceBusy = false;

    ScreenRefresh *m_screenRefresh = nullptr;

    void readPerformancePresets(std::function<void()> done);
    void refreshPerformanceState();
    void setPerformanceBusy(bool busy);
    void sendPerformanceRequests(QList<QJsonObject> requests, std::function<void(bool ok, const QJsonObject &reply)> done);
    QPointer<PulseAudioQt::Sink> m_defaultAudioSink;
    QList<QPointer<QQuickItem>> m_watchedGeometryItems;
    CalendarFeeds *m_calendarFeeds = nullptr;
    bool m_watchingPanelGeometry = false;
};
