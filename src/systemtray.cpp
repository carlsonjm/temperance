/*
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <algorithm>
#include <optional>

#include "config-X11.h"
#include "debug.h"
#include "systemtray.h"

#include "plasmoidregistry.h"
#include "sortedsystemtraymodel.h"
#include "statusnotifieritemhost.h"
#include "statusnotifieritemsource.h"
#include "systemtraymodel.h"
#include "systemtraysettings.h"

#include <QGuiApplication>
#include <QMenu>
#include <QMetaMethod>
#include <QMetaObject>
#include <QQueue>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QProcess>
#include <QScreen>
#include <QTimer>
#include <qpa/qplatformscreen.h>

#include <PulseAudioQt/Context>
#include <PulseAudioQt/Server>
#include <PulseAudioQt/Sink>
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
    setHasConfigurationInterface(true);
    setContainmentDisplayHints(Plasma::Types::ContainmentDrawsPlasmoidHeading | Plasma::Types::ContainmentForcesSquarePlasmoids);
}

SystemTray::~SystemTray()
{
    // When the applet is about to be deleted, delete now to avoid calling loadConfig()
    delete m_settings;
}

void SystemTray::init()
{
    // run this before doing any config restore
    migrateFromSystrayContainer();

    Containment::init();

    initSettingsAndRegistry();

    // Performance profiles are a capability-gated extension. A successful
    // helper discovery unlocks the control without creating a separate build
    // or exposing device-specific setup to everyone else.
    m_performanceHelper = QStandardPaths::findExecutable(u"z13ctl-plus"_s);
    if (!m_performanceHelper.isEmpty()) {
        refreshPerformancePresets();
        auto *presetRefreshTimer = new QTimer(this);
        presetRefreshTimer->setInterval(5000);
        connect(presetRefreshTimer, &QTimer::timeout, this, &SystemTray::refreshPerformancePresets);
        presetRefreshTimer->start();
    }

    // This applet replaces Plasma's notification presentation. Own the public
    // notification service directly so the stock popup UI is never created.
    NotificationManager::Server::self().init();

    auto *audioContext = PulseAudioQt::Context::instance();
    auto *audioServer = audioContext->server();
    if (audioServer) {
        connect(audioServer, &PulseAudioQt::Server::defaultSinkChanged, this, &SystemTray::updateDefaultAudioSink);
        updateDefaultAudioSink(audioServer->defaultSink());
    }

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

void SystemTray::updateDefaultAudioSink(PulseAudioQt::Sink *sink)
{
    if (m_defaultAudioSink == sink) {
        return;
    }
    if (m_defaultAudioSink) {
        disconnect(m_defaultAudioSink, nullptr, this, nullptr);
    }
    m_defaultAudioSink = sink;
    if (m_defaultAudioSink) {
        connect(m_defaultAudioSink, &PulseAudioQt::Sink::volumeChanged, this, &SystemTray::volumeChanged);
        connect(m_defaultAudioSink, &PulseAudioQt::Sink::mutedChanged, this, &SystemTray::volumeChanged);
    }
    Q_EMIT volumeChanged();
}

int SystemTray::volumePercent() const
{
    if (!m_defaultAudioSink) {
        return 0;
    }
    return qRound(100.0 * m_defaultAudioSink->volume() / PulseAudioQt::normalVolume());
}

bool SystemTray::volumeMuted() const
{
    return m_defaultAudioSink && m_defaultAudioSink->isMuted();
}

bool SystemTray::volumeAvailable() const
{
    return m_defaultAudioSink && m_defaultAudioSink->isVolumeWritable();
}

void SystemTray::setVolumePercent(int percent)
{
    if (!volumeAvailable()) {
        return;
    }
    const int bounded = std::clamp(percent, 0, 100);
    m_defaultAudioSink->setVolume(qRound64(PulseAudioQt::normalVolume() * bounded / 100.0));
}

QStringList SystemTray::performancePresets() const
{
    return m_performancePresets;
}

QString SystemTray::activePerformancePreset() const
{
    return m_activePerformancePreset;
}

void SystemTray::refreshPerformancePresets()
{
    if (m_performanceHelper.isEmpty()) {
        return;
    }
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int exitCode, QProcess::ExitStatus status) {
        if (status == QProcess::NormalExit && exitCode == 0) {
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
            if (presets != m_performancePresets) {
                m_performancePresets = presets;
                Q_EMIT performancePresetsChanged();
            }
            if (active != m_activePerformancePreset) {
                m_activePerformancePreset = active;
                Q_EMIT activePerformancePresetChanged();
            }
        }
        process->deleteLater();
    });
    process->start(m_performanceHelper, {u"preset"_s, u"list"_s});
}

void SystemTray::applyPerformancePreset(const QString &name)
{
    if (m_performanceHelper.isEmpty() || name.isEmpty()
        || (!m_performancePresets.isEmpty() && !m_performancePresets.contains(name))) {
        return;
    }
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process](int, QProcess::ExitStatus) {
        process->deleteLater();
        QTimer::singleShot(300, this, &SystemTray::refreshPerformancePresets);
    });
    process->start(m_performanceHelper, {u"preset"_s, u"apply"_s, name});
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
    if (!visualParent) {
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
    const QRect screen = visualParent->window()->screen()->availableGeometry();
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
    minimumWidth = std::max(1, minimumWidth);
    gap = std::max(0, gap);

    if (!visualParent || !visualParent->window() || !containment()) {
        return minimumWidth;
    }

    QQuickItem *ownItem = PlasmaQuick::AppletQuickItem::itemForApplet(const_cast<SystemTray *>(this));
    if (!ownItem || ownItem->window() != visualParent->window()) {
        ownItem = visualParent;
    }

    const qreal ownLeft = ownItem->mapToScene(QPointF(0, 0)).x();
    const qreal ownRight = ownItem->mapToScene(QPointF(ownItem->width(), 0)).x();
    qreal nearestLeftEdge = -1;

    for (Plasma::Applet *applet : containment()->applets()) {
        if (!applet || applet == this || applet->destroyed()) {
            continue;
        }

        const QString pluginId = applet->pluginMetaData().pluginId();
        if (pluginId == u"org.kde.plasma.panelspacer"_s
            || pluginId == u"org.kde.plasma.marginsseparator"_s
            || !PlasmaQuick::AppletQuickItem::hasItemForApplet(applet)) {
            continue;
        }

        auto *item = PlasmaQuick::AppletQuickItem::itemForApplet(applet);
        if (!item || !item->isVisible() || item->window() != ownItem->window()) {
            continue;
        }

        const qreal itemRight = item->mapToScene(QPointF(item->width(), 0)).x();
        if (itemRight <= ownLeft + 1 && itemRight > nearestLeftEdge) {
            nearestLeftEdge = itemRight;
        }
    }

    if (nearestLeftEdge < 0) {
        return minimumWidth;
    }

    return std::max(minimumWidth,
                    static_cast<int>(std::floor(ownRight - nearestLeftEdge - gap)));
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
    if (!m_sortedSystemTrayModel) {
        m_sortedSystemTrayModel = new SortedSystemTrayModel(SortedSystemTrayModel::SortingType::SystemTray, this);
        m_sortedSystemTrayModel->setSourceModel(systemTrayModel());
    }
    return m_sortedSystemTrayModel;
}

QAbstractItemModel *SystemTray::configSystemTrayModel()
{
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

void SystemTray::startApplet(const QString &pluginId)
{
    if (pluginId == u"org.kde.plasma.notifications"_s) {
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
        Applet *applet = Plasma::PluginLoader::self()->loadApplet(pluginId, m_configGroupIds.value(pluginId), QVariantList());
        // this should never happen unless explicitly wrong config is hand-written or
        //(more likely) a previously added applet is uninstalled
        if (!applet) {
            qCWarning(SYSTEM_TRAY) << "Unable to find applet" << pluginId;
            return;
        }
        applet->setProperty("org.kde.plasma:force-create", true);
        addApplet(applet);
        adoptExistingWeatherStation(applet);
        // create a new one automatic id, new config group
    } else {
        Applet *applet = createApplet(pluginId, QVariantList() << u"org.kde.plasma:force-create"_s);
        if (applet) {
            m_configGroupIds[pluginId] = applet->id();
            adoptExistingWeatherStation(applet);
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
        [this, statusNotifierIcon, pos](QMenu *menu) {
            if (menu && !menu->isEmpty()) {
                KAcceleratorManager::manage(menu);

                if (KWindowSystem::isPlatformWayland()) {
                    showSystemTrayMenuWayland(menu, statusNotifierIcon, location());
                } else {
                    showSystemTrayMenuX11(menu, statusNotifierIcon, pos, location());

                    // Workaround for QTBUG-59044
                    if (auto item = statusNotifierIcon->window()->mouseGrabberItem()) {
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

K_PLUGIN_CLASS_WITH_JSON(SystemTray, "metadata.json")

#include "systemtray.moc"

#include "moc_systemtray.cpp"
