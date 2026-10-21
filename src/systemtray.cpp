/*
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <algorithm>
#include <memory>
#include <type_traits>
#include <optional>

#include "config-X11.h"
#include "buildmatch.h"
#include "debug.h"
#include "systemtray.h"
#include "shared/SuitePresence.h"
#include "shared/SuiteSettings.h"
#include <KLocalizedString>

#include "audiostatus.h"
#include "calendarfeeds.h"
#include "clockextent.h"
#include "performancecontroller.h"
#include "corefocus.h"
#include "sessionactions.h"

#include <QScreen>
#include <optional>

#include "plasmoidregistry.h"
#include "sortedsystemtraymodel.h"
#include "statusnotifieritemhost.h"
#include "statusnotifieritemsource.h"
#include "systemtraymodel.h"
#include "systemtraysettings.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QMenu>
#include <QMetaMethod>
#include <QMetaObject>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQueue>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <qpa/qplatformscreen.h>

#include <notificationmanager/server.h>

#include <Plasma/Applet>
#include <Plasma/Corona>
#include <Plasma/PluginLoader>
#include <PlasmaQuick/AppletQuickItem>

#include <KAcceleratorManager>
#include <KActionCollection>
#include <KApplicationTrader>
#include <KIO/ApplicationLauncherJob>
#include <KService>
#include <KSharedConfig>
#include <KWaylandExtras>
#include <KWindowSystem>

using namespace Qt::StringLiterals;

// What this build was made for, in a form the installer reads from the file
// to warn before it installs a build the running Plasma or Qt will not run.
[[gnu::used]] static const char builtFor[] =
    "temperance-built-for plasma " TEMPERANCE_BUILT_PLASMA " qt " TEMPERANCE_BUILT_QT;

// Inside Shuffle the widget goes by Shuffle's name for it, in its title and
// its Configure entry; elsewhere it keeps its own.
static void useShuffleName(Plasma::Applet *applet, const QString &name)
{
    if (!suite::shuffleInstalled()) {
        return;
    }
    applet->setTitle(name);
    if (QAction *configure = applet->internalAction(QStringLiteral("configure"))) {
        configure->setText(i18nc("@action:inmenu %1 is the widget's name", "Configure %1…", name));
    }
}

void SystemTray::launchApplication(const QString &desktopName)
{
    auto service = KService::serviceByDesktopName(desktopName);
    if (!service) {
        service = KService::serviceByStorageId(desktopName.endsWith(u".desktop"_s)
                                                   ? desktopName
                                                   : desktopName + u".desktop"_s);
    }
    if (!service) {
        qCWarning(SYSTEM_TRAY) << "Unable to find application:" << desktopName;
        return;
    }

    auto *job = new KIO::ApplicationLauncherJob(service, this);
    job->start();
}

QString SystemTray::resolveApplicationIcon(const QString &applicationName,
                                           const QString &desktopEntry,
                                           const QString &fallbackIcon) const
{
    KService::Ptr service;
    if (!desktopEntry.isEmpty()) {
        service = KService::serviceByDesktopName(desktopEntry);
        if (!service) {
            service = KService::serviceByStorageId(desktopEntry.endsWith(u".desktop"_s)
                                                       ? desktopEntry
                                                       : desktopEntry + u".desktop"_s);
        }
    }

    if (!service && !applicationName.isEmpty()) {
        const auto matches = KApplicationTrader::query([&applicationName](const KService::Ptr &candidate) {
            return candidate && candidate->name().compare(applicationName, Qt::CaseInsensitive) == 0;
        });
        if (!matches.isEmpty()) {
            service = matches.constFirst();
        }
    }

    if (service && !service->icon().isEmpty()) {
        return service->icon();
    }
    return fallbackIcon.isEmpty() ? u"notifications-symbolic"_s : fallbackIcon;
}

static bool findWeatherStation(const KConfigGroup &group, QMap<QString, QString> &station)
{
    if (group.readEntry(u"plugin"_s, QString()) == u"org.kde.plasma.weather"_s) {
        const KConfigGroup weatherStation = group.group(u"Configuration"_s).group(u"WeatherStation"_s);
        const auto entries = weatherStation.entryMap();
        if (!entries.value(u"provider"_s).isEmpty() && !entries.value(u"placeInfo"_s).isEmpty()) {
            station = entries;
            return true;
        }
    }

    const auto children = group.groupList();
    for (const QString &child : children) {
        if (findWeatherStation(group.group(child), station)) {
            return true;
        }
    }
    return false;
}

static void adoptExistingWeatherStation(Plasma::Applet *applet)
{
    if (!applet || applet->pluginMetaData().pluginId() != u"org.kde.plasma.weather"_s) {
        return;
    }

    KConfigGroup destination = applet->config().group(u"WeatherStation"_s);
    if (!destination.readEntry(u"provider"_s, QString()).isEmpty()
        && !destination.readEntry(u"placeInfo"_s, QString()).isEmpty()) {
        return;
    }

    const auto desktopConfig = KSharedConfig::openConfig(u"plasma-org.kde.plasma.desktop-appletsrc"_s);
    QMap<QString, QString> station;
    if (!findWeatherStation(KConfigGroup(desktopConfig, u"Containments"_s), station)) {
        return;
    }

    for (auto it = station.cbegin(); it != station.cend(); ++it) {
        destination.writeEntry(it.key(), it.value());
    }
    destination.sync();
    applet->configChanged();
    qCDebug(SYSTEM_TRAY) << "Adopted existing Weather station:" << station.value(u"placeDisplayName"_s);
}

// Plasma 6.8 hands a loaded applet over as a std::unique_ptr, which the
// containment then takes; earlier releases pass a raw pointer. Either way the
// containment owns the applet, and the caller gets it back to finish setting up.
template<typename Loaded>
static Plasma::Applet *addLoadedApplet(Plasma::Containment *containment, Loaded loaded)
{
    Plasma::Applet *applet = nullptr;
    if constexpr (std::is_pointer_v<Loaded>) {
        applet = loaded;
    } else {
        applet = loaded.get();
    }
    if (!applet) {
        return nullptr;
    }
    applet->setProperty("org.kde.plasma:force-create", true);
    if constexpr (std::is_pointer_v<Loaded>) {
        containment->addApplet(loaded);
    } else {
        containment->addApplet(std::move(loaded));
    }
    return applet;
}

static void showSystemTrayMenuX11(QMenu *menu, QQuickItem *trayItem, const QPoint &pos, Plasma::Types::Location location)
{
    int x = pos.x();
    int y = pos.y();

    // try tofind the icon screen coordinates, and adjust the position as a poor
    // man's popupPosition

    QRect screenItemRect(trayItem->mapToScene(QPointF(0, 0)).toPoint(), QSize(trayItem->width(), trayItem->height()));

    if (trayItem->window()) {
        screenItemRect.moveTopLeft(trayItem->window()->mapToGlobal(screenItemRect.topLeft()));
    }

    menu->adjustSize();

    switch (location) {
    case Plasma::Types::LeftEdge:
        x = screenItemRect.right();
        y = screenItemRect.top();
        break;
    case Plasma::Types::RightEdge:
        x = screenItemRect.left() - menu->width();
        y = screenItemRect.top();
        break;
    case Plasma::Types::TopEdge:
        x = screenItemRect.left();
        y = screenItemRect.bottom();
        break;
    case Plasma::Types::BottomEdge:
        x = screenItemRect.left();
        y = screenItemRect.top() - menu->height();
        break;
    default:
        x = screenItemRect.left();
        if (screenItemRect.top() - menu->height() >= trayItem->window()->screen()->geometry().top()) {
            y = screenItemRect.top() - menu->height();
        } else {
            y = screenItemRect.bottom();
        }
    }

    menu->winId();
    menu->windowHandle()->setTransientParent(trayItem->window());
    menu->popup(QPoint(x, y));
}

static void showSystemTrayMenuWayland(QMenu *menu, QQuickItem *trayItem, Plasma::Types::Location location)
{
    QWindow *trayWindow = trayItem->window();
    if (!trayWindow) {
        qCWarning(SYSTEM_TRAY) << menu << "has no parent window, this should never happen";
        return;
    }

    menu->winId();
    QWindow *menuWindow = menu->windowHandle();

    const QRect anchorRect = trayItem->mapRectToScene(QRectF(QPoint(0, 0), trayItem->size())).toRect();

    Qt::Edges anchor;
    Qt::Edges gravity;
    switch (location) {
    case Plasma::Types::Location::TopEdge:
        anchor = Qt::BottomEdge;
        gravity = Qt::BottomEdge;

        if (qGuiApp->isLeftToRight()) {
            anchor |= Qt::LeftEdge;
            gravity |= Qt::RightEdge;
        } else {
            anchor |= Qt::RightEdge;
            gravity |= Qt::LeftEdge;
        }
        break;

    case Plasma::Types::Location::BottomEdge:
        anchor = Qt::TopEdge;
        gravity = Qt::TopEdge;

        if (qGuiApp->isLeftToRight()) {
            anchor |= Qt::LeftEdge;
            gravity |= Qt::RightEdge;
        } else {
            anchor |= Qt::RightEdge;
            gravity |= Qt::LeftEdge;
        }
        break;

    case Plasma::Types::Location::LeftEdge:
        anchor = Qt::RightEdge | Qt::TopEdge;
        gravity = Qt::RightEdge | Qt::BottomEdge;
        break;

    case Plasma::Types::Location::RightEdge:
        anchor = Qt::LeftEdge | Qt::TopEdge;
        gravity = Qt::LeftEdge | Qt::BottomEdge;
        break;

    default:
        anchor = Qt::LeftEdge | Qt::BottomEdge;
        gravity = Qt::RightEdge | Qt::BottomEdge;
        break;
    }

    menuWindow->setTransientParent(trayWindow);

    menuWindow->setProperty("_q_waylandPopupAnchorRect", anchorRect);
    menuWindow->setProperty("_q_waylandPopupAnchor", QVariant::fromValue(anchor));
    menuWindow->setProperty("_q_waylandPopupGravity", QVariant::fromValue(gravity));

    menu->popup(trayWindow->screen()->geometry().topLeft());
}

SystemTray::SystemTray(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : Plasma::Containment(parent, data, args)
{
    // Plasma's own version is plasmashell's, which is plasma-workspace's.
    const QString runningPlasma = QCoreApplication::applicationVersion();
    const QString runningQt = QString::fromLatin1(qVersion());
    m_needsUpdate = !builtForRunning(QStringLiteral(TEMPERANCE_BUILT_PLASMA), runningPlasma,
                                     QStringLiteral(TEMPERANCE_BUILT_QT), runningQt);
    m_calendarFeeds = new CalendarFeeds(this);
    // Present on every build, so the bar's bindings always have them; a build
    // for another Plasma or Qt never starts them.
    m_performance = new PerformanceController(this);
    m_audio = new AudioStatus(this);
    m_session = new SessionActions(this);
    m_coreFocus = new CoreFocus(this);
    setContainmentDisplayHints(Plasma::Types::ContainmentDrawsPlasmoidHeading | Plasma::Types::ContainmentForcesSquarePlasmoids);
    if (m_needsUpdate) {
        qCWarning(SYSTEM_TRAY) << "Temperance was built for Plasma" << TEMPERANCE_BUILT_PLASMA << "and Qt" << TEMPERANCE_BUILT_QT
                               << "but Plasma" << runningPlasma << "and Qt" << runningQt
                               << "are running; it leaves notifications and the tray to Plasma until a rebuild";
        return;
    }
    m_session->start();
    m_coreFocus->start();
    setHasConfigurationInterface(true);
}

// Plasma's own Notifications widget, loaded only while Status's
// notifications are off.
static const QString plasmaNotificationsId = u"org.kde.plasma.notifications"_s;

SystemTray::~SystemTray()
{
    // When the applet is about to be deleted, delete now to avoid calling loadConfig()
    delete m_settings;
    // The applets outlive this part of the object; none may call back into it.
    for (Plasma::Applet *applet : applets()) {
        applet->disconnect(this);
    }
}

void SystemTray::init()
{
    // Nothing below may run on a Plasma or Qt this build was not made for.
    if (m_needsUpdate) return initNeedingUpdate();

    // run this before doing any config restore
    migrateFromSystrayContainer();

    Containment::init();
    suite::openConfigureInSuiteSettings(this, QStringLiteral("status"));
    useShuffleName(this, i18nc("@title Shuffle's name for this widget", "Status Bar"));

    initSettingsAndRegistry();

    m_performance->start();

    // This applet replaces Plasma's notification presentation. Own the public
    // notification service directly so the stock popup UI is never created.
    // Another notification daemon may hold the service already; the history
    // then says who shows notifications instead of staying silently empty.
    if (!NotificationManager::Server::self().init()) {
        qCWarning(SYSTEM_TRAY) << "Another program holds the notification service;"
                               << "Temperance shows no notifications while it does";
    }

    m_audio->start();

    // we don't want to automatically propagate the activated signal from the Applet to the Containment
    // even if SystemTray is of type Containment, it is de facto Applet and should act like one
    connect(this, &Containment::appletAdded, this, [this](Plasma::Applet *applet) {
        disconnect(applet, &Applet::activated, this, &Applet::activated);
    });

    if (KWindowSystem::isPlatformWayland()) {
        auto config = KSharedConfig::openConfig(QStringLiteral("kdeglobals"), KConfig::NoGlobals);
        KConfigGroup kscreenGroup = config->group(QStringLiteral("KScreen"));
        m_xwaylandClientsScale = kscreenGroup.readEntry("XwaylandClientsScale", true);

        m_configWatcher = KConfigWatcher::create(config);
        connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group, const QByteArrayList &names) {
            if (group.name() == u"KScreen" && names.contains(QByteArrayLiteral("XwaylandClientsScale"))) {
                m_xwaylandClientsScale = group.readEntry("XwaylandClientsScale", true);
            }
        });
    }
}

// Plasma's public parts only: no notification service, tray, session controls
// or settings, so the notification service stays free for Plasma's own.
void SystemTray::initNeedingUpdate()
{
    Containment::init();
    useShuffleName(this, i18nc("@title Shuffle's name for this widget", "Status Bar"));
}

bool SystemTray::needsUpdate() const
{
    return m_needsUpdate;
}

void SystemTray::initSettingsAndRegistry()
{
    if (!m_settings) {
        m_settings = new SystemTraySettings(configScheme(), this);
        connect(m_settings, &SystemTraySettings::enabledPluginsChanged, this, &SystemTray::onEnabledAppletsChanged);
    }

    if (!m_plasmoidRegistry) {
        m_plasmoidRegistry = new PlasmoidRegistry(m_settings, this);
        connect(m_plasmoidRegistry, &PlasmoidRegistry::plasmoidEnabled, this, &SystemTray::startApplet);
        connect(m_plasmoidRegistry, &PlasmoidRegistry::plasmoidStopped, this, &SystemTray::stopApplet);
    }
}

void SystemTray::migrateFromSystrayContainer()
{
    // Search the old systray containment config group
    KConfigGroup rootCg(corona()->config(), QStringLiteral("Containments"));
    // NOTE: this function is called from the constructor, so we can't use config() yet
    KConfigGroup ownCg = KConfigGroup(config());
    // old Configuration group of the old systray applet
    KConfigGroup oldAppletCg(&ownCg, QStringLiteral("Configuration"));
    const uint oldSystrayId = oldAppletCg.readEntry(QStringLiteral("SystrayContainmentId"), 0);

    if (oldSystrayId == 0) {
        return;
    }

    // Config group of the old systray containment
    KConfigGroup oldContCg = KConfigGroup(&rootCg, QString::number(oldSystrayId));

    // Copy everything all the applets inside, all the subgroup of any applet
    QQueue<QPair<KConfigGroup, KConfigGroup>> queue;
    queue.enqueue(qMakePair(oldContCg, ownCg));
    // Iterative Tree traversal of the systray configuration
    while (!queue.isEmpty()) {
        QPair<KConfigGroup, KConfigGroup> current = queue.dequeue();
        const KConfigGroup &currentSource = current.first;
        KConfigGroup &currentDest = current.second;

        // Copy all entries in the current group
        const QMap<QString, QString> entries = currentSource.entryMap();
        for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
            currentDest.writeEntry(it.key(), currentSource.readEntry(it.key(), QString()));
        }

        const QStringList groups = currentSource.groupList();
        for (const QString &group : groups) {
            KConfigGroup sourceSubGroup = currentSource.group(group);
            KConfigGroup destSubGroup = currentDest.group(group);
            queue.enqueue(qMakePair(sourceSubGroup, destSubGroup));
        }
    }

    // Delete the the old systray
    for (Plasma::Containment *cont : corona()->containments()) {
        if (cont->id() == oldSystrayId) {
            delete cont;
            break;
        }
    }
    // To make sure the old containment is really gone from the config first delete then
    // remove the group, as destroy() might fail
    rootCg.deleteGroup(QString::number(oldSystrayId));

    // Delete the old unused key SystrayContainmentId in this systray
    oldAppletCg.deleteEntry(QStringLiteral("SystrayContainmentId"));

    for (Applet *a : applets()) {
        a->configChanged();
    }
}

void SystemTray::restoreContents(KConfigGroup &group)
{
    if (!isContainment()) {
        qCWarning(SYSTEM_TRAY) << "Loaded as an applet, this shouldn't have happened";
        return;
    }
    // A build for another Plasma or Qt hosts no tray.
    if (m_needsUpdate) {
        return;
    }

    KConfigGroup shortcutConfig(&group, u"Shortcuts"_s);
    QString shortcutText = shortcutConfig.readEntryUntranslated("global", QString());
    if (!shortcutText.isEmpty()) {
        setGlobalShortcut(QKeySequence(shortcutText));
    }

    // cache known config group ids for applets
    KConfigGroup cg = group.group(u"Applets"_s);
    for (const QString &group : cg.groupList()) {
        KConfigGroup appletConfig(&cg, group);
        QString plugin = appletConfig.readEntry("plugin");
        if (!plugin.isEmpty()) {
            m_configGroupIds[plugin] = group.toInt();
        }
    }

    initSettingsAndRegistry();
    m_plasmoidRegistry->init();
}

void SystemTray::showPlasmoidMenu(QQuickItem *appletInterface, int x, int y)
{
    if (!appletInterface) {
        return;
    }

    auto *applet = appletInterface->property("_plasma_applet").value<Plasma::Applet *>();

    QPointF pos = appletInterface->mapToScene(QPointF(x, y));

    if (appletInterface->window() && appletInterface->window()->screen()) {
        pos = appletInterface->window()->mapToGlobal(pos.toPoint());
    } else {
        pos = QPoint();
    }

    auto *desktopMenu = new QMenu;
    // Breeze and Oxygen have rounded corners on menus. They set this attribute
    // in polish() but at that time the underlying surface has already been
    // created so setting this flag makes no difference anymore (Bug 385311)
    desktopMenu->setAttribute(Qt::WA_TranslucentBackground);
    connect(this, &QObject::destroyed, desktopMenu, &QMenu::close);
    desktopMenu->setAttribute(Qt::WA_DeleteOnClose);

    // this is a workaround where Qt will fail to realize a mouse has been released

    // this happens if a window which does not accept focus spawns a new window that takes focus and X grab
    // whilst the mouse is depressed
    // https://bugreports.qt.io/browse/QTBUG-59044
    // this causes the next click to go missing

    // by releasing manually we avoid that situation
    auto ungrabMouseHack = [appletInterface]() {
        if (appletInterface->window() && appletInterface->window()->mouseGrabberItem()) {
            appletInterface->window()->mouseGrabberItem()->ungrabMouse();
        }
    };

    QTimer::singleShot(0, appletInterface, ungrabMouseHack);
    // end workaround

    Q_EMIT applet->contextualActionsAboutToShow();
    const auto contextActions = applet->contextualActions();
    for (QAction *action : contextActions) {
        if (action) {
            desktopMenu->addAction(action);
        }
    }

    if (applet->internalAction(QStringLiteral("configure"))) {
        desktopMenu->addAction(applet->internalAction(QStringLiteral("configure")));
    }

    if (desktopMenu->isEmpty()) {
        delete desktopMenu;
        return;
    }

    KAcceleratorManager::manage(desktopMenu);

    if (KWindowSystem::isPlatformWayland()) {
        showSystemTrayMenuWayland(desktopMenu, appletInterface, location());
    } else {
        showSystemTrayMenuX11(desktopMenu, appletInterface, pos.toPoint(), location());
    }
}

QPointF SystemTray::popupPosition(QQuickItem *visualParent, int x, int y)
{
    // The native geometry below is Qt's private API.
    if (!visualParent || m_needsUpdate) {
        return {0, 0};
    }

    QPointF pos = visualParent->mapToScene(QPointF(x, y));

    QQuickWindow *const window = visualParent->window();
    if (window && window->screen()) {
        pos = window->mapToGlobal(pos.toPoint());
#if HAVE_X11
        if (KWindowSystem::isPlatformX11()) {
            const auto devicePixelRatio = window->screen()->devicePixelRatio();
            if (QGuiApplication::screens().size() == 1) {
                return pos * devicePixelRatio;
            }

            const QRect geometry = window->screen()->geometry();
            const QRect nativeGeometry = window->screen()->handle()->geometry();
            const QPointF nativeGlobalPosOnCurrentScreen = (pos - geometry.topLeft()) * devicePixelRatio;

            return nativeGeometry.topLeft() + nativeGlobalPosOnCurrentScreen;
        }
#endif

        if (KWindowSystem::isPlatformWayland()) {
            if (!m_xwaylandClientsScale) {
                return pos;
            }

            const qreal devicePixelRatio = window->devicePixelRatio();

            if (QGuiApplication::screens().size() == 1) {
                return pos * devicePixelRatio;
            }

            const QRect geometry = window->screen()->geometry();
            const QRect nativeGeometry = window->screen()->handle()->geometry();
            const QPointF nativeGlobalPosOnCurrentScreen = (pos - geometry.topLeft()) * devicePixelRatio;

            return nativeGeometry.topLeft() + nativeGlobalPosOnCurrentScreen;
        }
    }

    return QPoint();
}

int SystemTray::availablePopupHeight(QQuickItem *visualParent) const
{
    if (!visualParent || !visualParent->window() || !visualParent->window()->screen())
        return 600;
    QScreen *const qscreen = visualParent->window()->screen();
    QRect screen = qscreen->availableGeometry();
    // On Wayland Qt reports the whole output as available: panels' reserved
    // edges, such as a dock along the bottom, are known only to the shell.
    // Its own room for windows is the one to stay inside, so the popup stops
    // above a dock rather than running under it.
    if (const Plasma::Corona *const shell = corona()) {
        for (int id = 0, count = int(shell->numScreens()); id < count; ++id) {
            if (shell->screenGeometry(id) != qscreen->geometry())
                continue;
            const QRect room = shell->availableScreenRect(id);
            if (room.isValid())
                screen = screen.intersected(room);
            break;
        }
    }
    const QPointF top = visualParent->mapToGlobal(QPointF(0, 0));
    const QPointF bottom = visualParent->mapToGlobal(QPointF(0, visualParent->height()));
    qreal space = screen.height();
    if (location() == Plasma::Types::BottomEdge)
        space = top.y() - screen.top();
    else if (location() == Plasma::Types::TopEdge)
        space = screen.y() + screen.height() - bottom.y();
    return std::max(1, int(space) - 20);
}

int SystemTray::availablePanelWidth(QQuickItem *visualParent, int minimumWidth, int gap) const
{
    return suite::panelRoom(const_cast<SystemTray *>(this), visualParent, minimumWidth, gap, suite::Toward::Before);
}

void SystemTray::watchHostClock(const QString &outputName)
{
    if (m_needsUpdate) {
        return;
    }
    if (!m_clockExtent) {
        m_clockExtent = new ClockExtent(this);
        connect(m_clockExtent, &ClockExtent::availableWidthChanged, this, &SystemTray::hostAvailableWidthChanged);
    }
    m_clockExtent->setOutput(outputName);
}

int SystemTray::hostAvailableWidth() const
{
    return m_clockExtent ? m_clockExtent->availableWidth() : 0;
}

void SystemTray::watchPanelGeometry(QQuickItem *visualParent)
{
    m_panelWatch.follow(this, visualParent, [this] {
        Q_EMIT panelGeometryChanged();
    });
}

bool SystemTray::isSystemTrayApplet(const QString &appletId)
{
    if (m_plasmoidRegistry) {
        return m_plasmoidRegistry->isSystemTrayApplet(appletId);
    }
    return false;
}

SystemTrayModel *SystemTray::systemTrayModel()
{
    if (!m_systemTrayModel) {
        m_systemTrayModel = new SystemTrayModel(this);

        m_plasmoidModel = new PlasmoidModel(m_settings, m_plasmoidRegistry, m_systemTrayModel);
        connect(this, &SystemTray::appletAdded, m_plasmoidModel, &PlasmoidModel::addApplet);
        connect(this, &SystemTray::appletRemoved, m_plasmoidModel, &PlasmoidModel::removeApplet);
        for (auto applet : applets()) {
            m_plasmoidModel->addApplet(applet);
        }

        m_statusNotifierModel = new StatusNotifierModel(m_settings, m_systemTrayModel);

        m_systemTrayModel->addSourceModel(m_plasmoidModel);
        m_systemTrayModel->addSourceModel(m_statusNotifierModel);
        m_systemTrayModel->addSourceModel(new BackgroundAppsFilteredModel(new BackgroundAppsModel(m_settings, m_systemTrayModel), m_systemTrayModel));
    }

    return m_systemTrayModel;
}

QAbstractItemModel *SystemTray::sortedSystemTrayModel()
{
    // A build for another Plasma or Qt hosts no tray.
    if (m_needsUpdate) {
        return nullptr;
    }
    if (!m_sortedSystemTrayModel) {
        m_sortedSystemTrayModel = new SortedSystemTrayModel(SortedSystemTrayModel::SortingType::SystemTray, this);
        m_sortedSystemTrayModel->setSourceModel(systemTrayModel());
    }
    return m_sortedSystemTrayModel;
}

QAbstractItemModel *SystemTray::configSystemTrayModel()
{
    // A build for another Plasma or Qt hosts no tray.
    if (m_needsUpdate) {
        return nullptr;
    }
    if (!m_configSystemTrayModel) {
        m_configSystemTrayModel = new SortedSystemTrayModel(SortedSystemTrayModel::SortingType::ConfigurationPage, this);
        m_configSystemTrayModel->setSourceModel(systemTrayModel());
    }
    return m_configSystemTrayModel;
}

void SystemTray::onEnabledAppletsChanged()
{
    // remove all that are not allowed anymore
    const auto appletsList = applets();
    for (Plasma::Applet *applet : appletsList) {
        // Here it should always be valid.
        // for some reason it not always is.
        if (!applet->pluginMetaData().isValid()) {
            applet->config().parent().deleteGroup();
            delete applet;
        } else {
            const QString task = applet->pluginMetaData().pluginId();
            // Shown in Status's place, whatever the tray's own list says.
            if (task == plasmaNotificationsId && m_plasmaNotificationsWanted) {
                continue;
            }
            if (!m_settings->isEnabledPlugin(task)) {
                // in those cases we do delete the applet config completely
                // as they were explicitly disabled by the user
                applet->config().parent().deleteGroup();
                delete applet;
                m_configGroupIds.remove(task);
            }
        }
    }
}

bool SystemTray::plasmaNotificationsWanted() const
{
    return m_plasmaNotificationsWanted;
}

bool SystemTray::plasmaNotificationsShown() const
{
    return m_plasmaNotificationsShown;
}

// Both read the same notifications from the one server in this process, so
// switching changes only who shows them: the service, the history and Do Not
// Disturb stay as they are.
void SystemTray::setPlasmaNotificationsWanted(bool wanted)
{
    if (m_needsUpdate || wanted == m_plasmaNotificationsWanted) {
        return;
    }
    m_plasmaNotificationsWanted = wanted;
    Q_EMIT plasmaNotificationsWantedChanged();
    if (wanted) {
        startApplet(plasmaNotificationsId);
    } else {
        // Gone before Status shows notifications again, so none shows twice.
        stopApplet(plasmaNotificationsId);
        setPlasmaNotificationsShown(false);
    }
}

void SystemTray::setPlasmaNotificationsShown(bool shown)
{
    if (shown == m_plasmaNotificationsShown) {
        return;
    }
    m_plasmaNotificationsShown = shown;
    Q_EMIT plasmaNotificationsShownChanged();
}

// The widget takes over the popups when its page is complete, which making
// its item does at once. Until then, and if it fails or goes away, Status
// keeps showing notifications.
void SystemTray::watchPlasmaNotifications(Plasma::Applet *applet)
{
    connect(applet, &QObject::destroyed, this, [this]() {
        // Once now and once its item is surely gone, whichever goes last.
        releasePlasmaNotificationPopups();
        QTimer::singleShot(0, this, &SystemTray::releasePlasmaNotificationPopups);
        setPlasmaNotificationsShown(false);
    });
    QTimer::singleShot(0, applet, [this, applet]() {
        if (!m_plasmaNotificationsWanted) {
            return;
        }
        if (auto *item = PlasmaQuick::AppletQuickItem::itemForApplet(applet)) {
            m_plasmaNotificationsEngine = qmlEngine(item);
            setPlasmaNotificationsShown(true);
        } else {
            qCWarning(SYSTEM_TRAY) << "Plasma's Notifications widget did not load;"
                                   << "Status keeps showing notifications";
        }
    });
}

// The widget's popups live in a singleton of its own that outlives it. When
// the last such widget goes, the singleton fails before it lets go of it, so
// its popups keep coming, placed at the screen's corner. Let go of it here,
// unless another Notifications widget elsewhere still shows them.
void SystemTray::releasePlasmaNotificationPopups()
{
    if (!m_plasmaNotificationsEngine || QCoreApplication::closingDown()) {
        return;
    }
    QQmlComponent release(m_plasmaNotificationsEngine);
    release.setData(QByteArrayLiteral("import QtQml\n"
                                      "import plasma.applet.org.kde.plasma.notifications as Notifications\n"
                                      "QtObject {\n"
                                      "    Component.onCompleted: {\n"
                                      "        if (Notifications.Globals.plasmoidItems.length === 0) {\n"
                                      "            Notifications.Globals.plasmoidItem = null;\n"
                                      "            Notifications.Globals.plasmoid = null;\n"
                                      "        }\n"
                                      "    }\n"
                                      "}\n"),
                    QUrl());
    const std::unique_ptr<QObject> done(release.create());
    if (!done) {
        qCWarning(SYSTEM_TRAY) << "Could not release Plasma's notification popups:" << release.errorString();
    }
}

void SystemTray::startApplet(const QString &pluginId)
{
    if (pluginId == plasmaNotificationsId && !m_plasmaNotificationsWanted) {
        return;
    }
    const auto appletsList = applets();
    for (Plasma::Applet *applet : appletsList) {
        if (!applet->pluginMetaData().isValid()) {
            continue;
        }

        // only allow one instance per applet
        if (pluginId == applet->pluginMetaData().pluginId()) {
            // Applet::destroy doesn't delete the applet from Containment::applets in the same event
            // potentially a dbus activated service being restarted can be added in this time.
            if (!applet->destroyed()) {
                return;
            }
        }
    }

    qCDebug(SYSTEM_TRAY) << "Adding applet:" << pluginId;

    // known one, recycle the id to reuse old config
    if (m_configGroupIds.contains(pluginId)) {
        Applet *applet = addLoadedApplet(
            this, Plasma::PluginLoader::self()->loadApplet(pluginId, m_configGroupIds.value(pluginId), QVariantList()));
        // this should never happen unless explicitly wrong config is hand-written or
        //(more likely) a previously added applet is uninstalled
        if (!applet) {
            qCWarning(SYSTEM_TRAY) << "Unable to find applet" << pluginId;
            return;
        }
        adoptExistingWeatherStation(applet);
        if (pluginId == plasmaNotificationsId) {
            watchPlasmaNotifications(applet);
        }
        // create a new one automatic id, new config group
    } else {
        Applet *applet = createApplet(pluginId, QVariantList() << u"org.kde.plasma:force-create"_s);
        if (applet) {
            m_configGroupIds[pluginId] = applet->id();
            adoptExistingWeatherStation(applet);
            if (pluginId == plasmaNotificationsId) {
                watchPlasmaNotifications(applet);
            }
        } else if (pluginId == plasmaNotificationsId) {
            qCWarning(SYSTEM_TRAY) << "Plasma's Notifications widget is not installed;"
                                   << "Status keeps showing notifications";
        }
    }
}

void SystemTray::stopApplet(const QString &pluginId)
{
    const auto appletsList = applets();
    for (Plasma::Applet *applet : appletsList) {
        if (applet->pluginMetaData().isValid() && pluginId == applet->pluginMetaData().pluginId()) {
            delete applet;
        }
    }
}

void SystemTray::stackItemBefore(QQuickItem *newItem, QQuickItem *beforeItem)
{
    if (!newItem || !beforeItem) {
        return;
    }
    newItem->stackBefore(beforeItem);
}

void SystemTray::stackItemAfter(QQuickItem *newItem, QQuickItem *afterItem)
{
    if (!newItem || !afterItem) {
        return;
    }
    newItem->stackAfter(afterItem);
}

void SystemTray::activate(const QString &service, QPoint pos, QQuickItem *statusNotifierIcon)
{
    const auto source = StatusNotifierItemHost::self()->itemForService(service);

    if (!source) {
        qCWarning(SYSTEM_TRAY) << "activate: Could not find item for service" << service;
        return;
    }

    connect(
        source,
        &StatusNotifierItemSource::activateResult,
        this,
        [this, service, pos, statusNotifierIcon](bool res) {
            if (!res) {
                // On error try to invoke the context menu.
                // Workaround primarily for apps using libappindicator.
                openContextMenu(service, pos, statusNotifierIcon);
            }
        },
        Qt::SingleShotConnection);

    QWindow *window = nullptr;
    if (KWindowSystem::isPlatformX11()) {
        source->activate(pos.x(), pos.y());
        return;
    }

    auto tokenFuture = KWaylandExtras::xdgActivationToken(window, {});
    tokenFuture.then(source, [source, service, pos](const QString &token) {
        source->provideXdgActivationToken(token);
        source->activate(pos.x(), pos.y());
    });
}

void SystemTray::secondaryActivate(const QString &service, QPoint pos)
{
    const auto source = StatusNotifierItemHost::self()->itemForService(service);

    if (!source) {
        qCWarning(SYSTEM_TRAY) << "secondaryActivate: Could not find item for service" << service;
        return;
    }

    QWindow *window = nullptr;
    if (KWindowSystem::isPlatformX11()) {
        source->secondaryActivate(pos.x(), pos.y());
        return;
    }

    auto tokenFuture = KWaylandExtras::xdgActivationToken(window, {});
    tokenFuture.then(source, [source, pos](const QString &token) {
        source->provideXdgActivationToken(token);
        source->secondaryActivate(pos.x(), pos.y());
    });
}

void SystemTray::openContextMenu(const QString &service, QPoint pos, QQuickItem *statusNotifierIcon)
{
    const auto source = StatusNotifierItemHost::self()->itemForService(service);

    if (!source) {
        qCWarning(SYSTEM_TRAY) << "openContextMenu: Could not find item for service" << service;
        return;
    }

    connect(
        source,
        &StatusNotifierItemSource::contextMenuReady,
        this,
        // The icon can go before its menu arrives, when its application quits
        // or the item hides, so it is held weakly.
        [this, icon = QPointer<QQuickItem>(statusNotifierIcon), pos](QMenu *menu) {
            if (icon && menu && !menu->isEmpty()) {
                KAcceleratorManager::manage(menu);

                if (KWindowSystem::isPlatformWayland()) {
                    showSystemTrayMenuWayland(menu, icon, location());
                } else {
                    showSystemTrayMenuX11(menu, icon, pos, location());

                    // Workaround for QTBUG-59044
                    if (auto item = icon->window() ? icon->window()->mouseGrabberItem() : nullptr) {
                        item->ungrabMouse();
                    }
                }
            }
        },
        Qt::SingleShotConnection);

    QWindow *window = nullptr;
    if (KWindowSystem::isPlatformX11()) {
        source->contextMenu(pos.x(), pos.y());
        return;
    }

    auto tokenFuture = KWaylandExtras::xdgActivationToken(window, {});
    tokenFuture.then(source, [source, pos](const QString &token) {
        source->provideXdgActivationToken(token);
        source->contextMenu(pos.x(), pos.y());
    });
}

void SystemTray::scroll(const QString &service, int delta, const QString &direction)
{
    const auto source = StatusNotifierItemHost::self()->itemForService(service);

    if (!source) {
        qCWarning(SYSTEM_TRAY) << "scroll: Could not find item for service" << service;
        return;
    }

    source->scroll(delta, direction);
}

CalendarFeeds *SystemTray::calendarFeeds() const
{
    return m_calendarFeeds;
}

PerformanceController *SystemTray::performance() const
{
    return m_performance;
}

AudioStatus *SystemTray::audio() const
{
    return m_audio;
}

SessionActions *SystemTray::session() const
{
    return m_session;
}

CoreFocus *SystemTray::coreFocus() const
{
    return m_coreFocus;
}

K_PLUGIN_CLASS_WITH_JSON(SystemTray, "metadata.json")

#include "systemtray.moc"

#include "moc_systemtray.cpp"
