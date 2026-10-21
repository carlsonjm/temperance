/*
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractItemModel>
#include <QVariantMap>
#include <QPointer>
#include <QList>

#include <KConfigWatcher>

#include <Plasma/Containment>

#include "calendarfeeds.h"
#include "shared/PanelRoom.h"

class QQuickItem;

namespace Plasma
{
class Applet;
}
class PlasmoidRegistry;
class PlasmoidModel;
class SystemTraySettings;
class StatusNotifierModel;
class SystemTrayModel;
class SortedSystemTrayModel;
class KJob;
class AudioStatus;
class PerformanceController;
class SessionActions;
class ClockExtent;


class SystemTray : public Plasma::Containment
{
    Q_OBJECT
    Q_MOC_INCLUDE("audiostatus.h")
    Q_MOC_INCLUDE("performancecontroller.h")
    Q_MOC_INCLUDE("sessionactions.h")
    Q_PROPERTY(QAbstractItemModel *systemTrayModel READ sortedSystemTrayModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *configSystemTrayModel READ configSystemTrayModel CONSTANT)
    // Temperance's own parts, kept apart from the tray host inherited from
    // Plasma: performance profiles, the volume and the session controls.
    Q_PROPERTY(PerformanceController *performance READ performance CONSTANT)
    Q_PROPERTY(AudioStatus *audio READ audio CONSTANT)
    Q_PROPERTY(SessionActions *session READ session CONSTANT)
    // The calendars linked in the settings, shared by the calendar card and
    // the settings page that shows each link's state.
    Q_PROPERTY(CalendarFeeds *calendarFeeds READ calendarFeeds CONSTANT)
    // Built for another Plasma or Qt than the one running: the Status Bar
    // shows only that it needs an update (buildmatch.h).
    Q_PROPERTY(bool needsUpdate READ needsUpdate CONSTANT)
    // The room a host leaves beside a clock it paints itself, in logical
    // pixels from the clock's edge to the output's far edge; 0 where no
    // host says (clockextent.h).
    Q_PROPERTY(int hostAvailableWidth READ hostAvailableWidth NOTIFY hostAvailableWidthChanged)
    // With Status's notifications off, Plasma's own Notifications widget is
    // asked for and shows them instead (ARCHITECTURE.md § Notifications).
    // Status keeps showing them until that widget is up, so none is missed.
    Q_PROPERTY(bool plasmaNotificationsWanted READ plasmaNotificationsWanted WRITE setPlasmaNotificationsWanted
                   NOTIFY plasmaNotificationsWantedChanged)
    Q_PROPERTY(bool plasmaNotificationsShown READ plasmaNotificationsShown NOTIFY plasmaNotificationsShownChanged)

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
    // Asks the host about the output the applet's window is on, once, and
    // again only when the output changes or the host says it moved.
    Q_INVOKABLE void watchHostClock(const QString &outputName);
    int hostAvailableWidth() const;

    bool plasmaNotificationsWanted() const;
    void setPlasmaNotificationsWanted(bool wanted);
    bool plasmaNotificationsShown() const;

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
    Q_INVOKABLE QString resolveApplicationIcon(const QString &applicationName,
                                               const QString &desktopEntry,
                                               const QString &fallbackIcon) const;

    PerformanceController *performance() const;
    AudioStatus *audio() const;
    SessionActions *session() const;
    CalendarFeeds *calendarFeeds() const;
    bool needsUpdate() const;

Q_SIGNALS:
    void panelGeometryChanged();
    void hostAvailableWidthChanged();
    void plasmaNotificationsWantedChanged();
    void plasmaNotificationsShownChanged();

private Q_SLOTS:
    // synchronizes with configuration and deletes not allowed applets
    void onEnabledAppletsChanged();
    // creates an applet *if not already existing*
    void startApplet(const QString &pluginId);
    // deletes/stops all instances of a given applet
    void stopApplet(const QString &pluginId);

private:
    bool m_needsUpdate = false;
    void initNeedingUpdate();
    void migrateFromSystrayContainer();
    SystemTrayModel *systemTrayModel();
    void initSettingsAndRegistry();
    void watchPlasmaNotifications(Plasma::Applet *applet);
    void setPlasmaNotificationsShown(bool shown);

    KConfigWatcher::Ptr m_configWatcher;
    bool m_xwaylandClientsScale = true;
    bool m_plasmaNotificationsWanted = false;
    bool m_plasmaNotificationsShown = false;

    QPointer<SystemTraySettings> m_settings;
    QPointer<PlasmoidRegistry> m_plasmoidRegistry;

    PlasmoidModel *m_plasmoidModel = nullptr;
    StatusNotifierModel *m_statusNotifierModel = nullptr;
    SystemTrayModel *m_systemTrayModel = nullptr;
    SortedSystemTrayModel *m_sortedSystemTrayModel = nullptr;
    SortedSystemTrayModel *m_configSystemTrayModel = nullptr;

    QHash<QString /*plugin id*/, int /*config group*/> m_configGroupIds;

    PerformanceController *m_performance = nullptr;
    AudioStatus *m_audio = nullptr;
    SessionActions *m_session = nullptr;
    suite::PanelWatch m_panelWatch;
    CalendarFeeds *m_calendarFeeds = nullptr;
    ClockExtent *m_clockExtent = nullptr;
};
