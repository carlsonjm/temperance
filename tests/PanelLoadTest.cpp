// SPDX-License-Identifier: LGPL-2.0-or-later
#include <QApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFile>
#include <QEventLoop>
#include <QImage>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QTimer>
#include <QQmlEngine>
#include <QQmlComponent>
#include <KPackage/Package>
#include <KConfigGroup>
#include <KPackage/PackageLoader>
#include <Plasma/Applet>
#include <Plasma/Containment>
#include <Plasma/Corona>
#include "CoronaScreen.h"
#include <PlasmaQuick/AppletQuickItem>
#include <memory>

class TestCorona : public Plasma::Corona {
public:
    QRect screenGeometry(CoronaScreenId) const override { return QRect(0, 0, 1280, 800); }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    TestCorona corona;
    auto shell = KPackage::PackageLoader::self()->loadPackage(QStringLiteral("Plasma/Shell"));
    shell.setPath(QStringLiteral("org.kde.plasma.desktop"));
    corona.setKPackage(shell);
    auto *panel = corona.createContainment(QStringLiteral("null"));
    if (!panel) return 1;
    panel->setFormFactor(Plasma::Types::Horizontal);
    panel->setLocation(Plasma::Types::BottomEdge);
    auto *applet = panel->createApplet(QStringLiteral("co.goodinput.temperance"));
    if (!applet || applet->failedToLaunch()) return 2;
    // Test Temperance itself, not embedded third-party tray applets which
    // may require real display-server services (for example Klipper).
    applet->config().writeEntry("extraItems", QStringList{});
    const QString expected = QString::fromLocal8Bit(qgetenv("QT_PLUGIN_PATH"))
        + QStringLiteral("/plasma/applets/co.goodinput.temperance.so");
    if (QFileInfo(applet->pluginMetaData().fileName()).canonicalFilePath()
        != QFileInfo(expected).canonicalFilePath()) return 3;
    // Load the complete packaged component graph without instantiating
    // embedded tray clients (Klipper needs a real display server).
    // Unlike qmlcachegen, this rejects assignments to read-only properties
    // in nested components before the package reaches the live panel.
    QQmlEngine engine;
    QFile source(QStringLiteral(":/qt/qml/plasma/applet/co/goodinput/temperance/main.qml"));
    if (!source.open(QIODevice::ReadOnly)) return 6;
    const QString qml = QString::fromUtf8(source.readAll());
    if (!qml.contains(QStringLiteral("Plasmoid.watchPanelGeometry(root)"))
        || !qml.contains(QStringLiteral("function onPanelGeometryChanged()"))) return 13;
    QString navigationFunctions;
    for (const auto &name : {"activateAppletById", "showApplet", "openSurface", "showSurface"}) {
        const auto begin = qml.indexOf(QStringLiteral("function ") + QString::fromLatin1(name) + QStringLiteral("("));
        const auto finish = qml.indexOf(QStringLiteral("\n    }"), begin);
        if (begin < 0 || finish < 0) return 11;
        navigationFunctions += qml.mid(begin, finish - begin + 6);
    }
    const auto navigationTest = engine.evaluate(QStringLiteral(R"JS(
        (function() {
            const appletsById = {weather: {}, audio: {}};
            const systemTrayState = {expanded: false, page: 'control', activeApplet: null,
                setActiveApplet: function(a) { this.activeApplet = a; if (a) this.expanded = true; }};
            let measured = 0;
            function refreshPopupHeightLimit() { ++measured; }
    )JS") + navigationFunctions + QStringLiteral(R"JS(
            openSurface('tray');
            if (!systemTrayState.expanded) throw 'open tray';
            if (measured !== 1) throw 'room measured as a page opens';
            openSurface('tray');
            if (systemTrayState.expanded) throw 'close tray';
            openSurface('notifications'); openSurface('control');
            if (!systemTrayState.expanded || systemTrayState.page !== 'control') throw 'switch';
            activateAppletById('weather'); activateAppletById('weather');
            if (systemTrayState.expanded) throw 'close weather';
            activateAppletById('weather'); openSurface('tray');
            if (!systemTrayState.expanded || systemTrayState.activeApplet) throw 'weather to tray';
            // Back returns to the page an applet was opened from; from the bar,
            // to Control Center.
            activateAppletById('audio');
            if (systemTrayState.page !== 'tray') throw 'tray applet backs to tray';
            systemTrayState.expanded = false; systemTrayState.page = 'notifications';
            activateAppletById('audio'); activateAppletById('weather');
            if (systemTrayState.page !== 'control') throw 'bar applet backs to Control Center';
            // Chosen from the slim row's strip, a page opens and stays open.
            showSurface('tray'); showSurface('tray');
            if (!systemTrayState.expanded || systemTrayState.page !== 'tray') throw 'strip keeps tray';
            showApplet('weather'); showApplet('weather');
            if (!systemTrayState.expanded || systemTrayState.activeApplet !== appletsById.weather)
                throw 'strip keeps weather';
            return true;
        })()
    )JS"));
    if (navigationTest.isError() || !navigationTest.toBool()) {
        qCritical() << "Panel navigation regression:" << navigationTest.toString();
        return 12;
    }
    // Which notifications take a banner: critical ones and a fresh logout
    // cancellation, never a normal one for having actions.
    QString routingFunctions;
    for (const auto &name : {"isFreshNotification", "isFreshLogoutCancellation", "isPriorityNotificationCandidate"}) {
        const auto begin = qml.indexOf(QStringLiteral("function ") + QString::fromLatin1(name) + QStringLiteral("("));
        const auto finish = qml.indexOf(QStringLiteral("\n    }"), begin);
        if (begin < 0 || finish < 0) return 21;
        routingFunctions += qml.mid(begin, finish - begin + 6);
    }
    const auto routingTest = engine.evaluate(QStringLiteral(R"JS(
        (function() {
            const NotificationManager = { Notifications: { UrgencyRole: 1, UpdatedRole: 2, CreatedRole: 3,
                SummaryRole: 4, BodyRole: 5, ActionNamesRole: 6, HasDefaultActionRole: 7,
                NormalUrgency: 1, CriticalUrgency: 2 } };
            const priorityAlertFreshnessMs = 20000;
            function model(fields) { return { data: (index, role) => fields[role] }; }
            const now = new Date();
            const old = new Date(Date.now() - 60000);
    )JS") + routingFunctions + QStringLiteral(R"JS(
            if (!isPriorityNotificationCandidate(model({1: 2, 2: now}), 0)) throw 'critical';
            if (isPriorityNotificationCandidate(model({1: 1, 2: now, 4: 'Download', 6: ['open'], 7: true}), 0))
                throw 'actionable normal';
            if (!isPriorityNotificationCandidate(model({1: 1, 2: now, 4: 'Logout canceled'}), 0)) throw 'logout cancelled';
            if (isPriorityNotificationCandidate(model({1: 1, 2: old, 4: 'Logout canceled'}), 0)) throw 'stale logout';
            return true;
        })()
    )JS"));
    if (routingTest.isError() || !routingTest.toBool()) {
        qCritical() << "Banner routing regression:" << routingTest.toString();
        return 21;
    }
    const auto acStart = qml.indexOf(QStringLiteral("function acHoldingCharge("));
    const auto acEnd = qml.indexOf(QStringLiteral("\n    }"), acStart);
    if (acStart < 0 || acEnd < 0) return 9;
    const auto acTest = engine.evaluate(QStringLiteral("(function() {")
        + qml.mid(acStart, acEnd - acStart + 6) + QStringLiteral(R"JS(
            if (!acHoldingCharge(false, 4) || !acHoldingCharge(false, 5)) throw 'AC hold';
            for (const state of [0, 1, 2, 3, 6, undefined])
                if (acHoldingCharge(false, state)) throw 'not holding';
            for (const power of [true, undefined, null])
                if (acHoldingCharge(power, 5)) throw 'AC unconfirmed';
            return true;
        })()
    )JS"));
    if (acTest.isError() || !acTest.toBool()) {
        qCritical() << "AC indicator regression:" << acTest.toString();
        return 10;
    }
    const auto start = qml.indexOf(QStringLiteral("function nextUnpresentedNotification()"));
    const auto end = qml.indexOf(QStringLiteral("\n    }"), start);
    if (start < 0 || end < 0) return 7;
    const QString function = qml.mid(start, end - start + 6);
    const auto queueTest = engine.evaluate(QStringLiteral(R"JS(
        (function() {
            let keys = ['a', 'b'];
            const railNotifications = { count: 2, index: row => row };
            const presentedNotificationKeys = {};
            function priorityNotificationKey(model, row) { return keys[row]; }
    )JS") + function + QStringLiteral(R"JS(
            if (nextUnpresentedNotification() !== 0) throw 'first item';
            presentedNotificationKeys.a = true;
            if (nextUnpresentedNotification() !== 1) throw 'second item';
            presentedNotificationKeys.b = true;
            if (nextUnpresentedNotification() !== -1) throw 'replay';
            railNotifications.count = 0;
            if (nextUnpresentedNotification() !== -1) throw 'empty reset';
            railNotifications.count = 2;
            if (nextUnpresentedNotification() !== -1) throw 'reset replay';
            keys = ['c', 'a', 'b']; railNotifications.count = 3;
            if (nextUnpresentedNotification() !== 0) throw 'new arrival';
            return true;
        })()
    )JS"));
    if (queueTest.isError() || !queueTest.toBool()) {
        qCritical() << "Notification identity regression:" << queueTest.toString();
        return 8;
    }
    QQmlComponent component(&engine,
        QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/co/goodinput/temperance/main.qml")),
        QQmlComponent::PreferSynchronous);
    if (!component.isReady()) {
        qCritical() << component.errors();
        return 4;
    }
    QQmlComponent iconComponent(&engine,
        QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/co/goodinput/temperance/SuiteIcon.qml")),
        QQmlComponent::PreferSynchronous);
    if (!iconComponent.isReady()) {
        qCritical() << iconComponent.errors();
        return 14;
    }
    QQmlComponent anchorComponent(&engine,
        QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/co/goodinput/temperance/PanelPopupAnchor.qml")),
        QQmlComponent::PreferSynchronous);
    if (!anchorComponent.isReady()) {
        qCritical() << anchorComponent.errors();
        return 18;
    }
    const auto makeAnchor = [&](Plasma::Types::Location location, bool vertical) {
        return std::unique_ptr<QObject>(anchorComponent.createWithInitialProperties({
            {QStringLiteral("vertical"), vertical}, {QStringLiteral("surfaceWidth"), 240.0},
            {QStringLiteral("surfaceHeight"), 48.0}, {QStringLiteral("panelLocation"), int(location)}
        }));
    };
    auto bottomAnchor = makeAnchor(Plasma::Types::BottomEdge, false);
    auto topAnchor = makeAnchor(Plasma::Types::TopEdge, false);
    auto leftAnchor = makeAnchor(Plasma::Types::LeftEdge, true);
    auto rightAnchor = makeAnchor(Plasma::Types::RightEdge, true);
    const auto *bottomItem = qobject_cast<QQuickItem *>(bottomAnchor.get());
    const auto *topItem = qobject_cast<QQuickItem *>(topAnchor.get());
    const auto *leftItem = qobject_cast<QQuickItem *>(leftAnchor.get());
    const auto *rightItem = qobject_cast<QQuickItem *>(rightAnchor.get());
    if (!bottomItem || !topItem || !leftItem || !rightItem
        || bottomItem->y() != -18 || topItem->y() != 18
        || leftItem->x() != 18 || rightItem->x() != -18
        || bottomItem->x() != 239 || bottomItem->width() != 1) {
        qCritical() << "Popup anchor does not provide the intended panel clearance";
        return 19;
    }
    QQuickWindow iconWindow;
    iconWindow.setGeometry(0, 0, 64, 64);
    iconWindow.setColor(Qt::transparent);
    iconWindow.show();
    const QStringList glyphs{
        QStringLiteral("ellipsis-vertical"), QStringLiteral("settings"), QStringLiteral("plus"),
        QStringLiteral("pin"), QStringLiteral("trash-2"), QStringLiteral("chevron-up"),
        QStringLiteral("chevron-down"), QStringLiteral("lock"), QStringLiteral("log-out"),
        QStringLiteral("rotate-cw"), QStringLiteral("moon"), QStringLiteral("users"),
        QStringLiteral("power"), QStringLiteral("sliders-horizontal"), QStringLiteral("sun"), QStringLiteral("cloud-sun"),
        QStringLiteral("gauge"), QStringLiteral("nm-signal-100-symbolic"),
        QStringLiteral("nm-signal-50-secure-symbolic"), QStringLiteral("audio-volume-high-symbolic"),
        QStringLiteral("bluetooth-active-symbolic")
    };
    // Every state Plasma's network widget reports, and the icons Plasma's own
    // status widgets give, is drawn by a drawing Temperance ships: none falls
    // back to the icon theme, which made Status look different on every
    // computer with a different theme or variant installed.
    QQmlComponent glyphMap(&engine);
    glyphMap.setData(QByteArrayLiteral(
        "import QtQuick\n"
        "import \"qrc:/qt/qml/plasma/applet/co/goodinput/temperance/StatusGlyphs.js\" as G\n"
        "QtObject {\n"
        "  function check(names, network) {\n"
        "    const out = [];\n"
        "    for (const name of names) out.push(network ? G.network(name) : G.applet(name));\n"
        "    return out;\n"
        "  }\n"
        "}\n"), QUrl(QStringLiteral("qrc:/temperance-glyph-map-test.qml")));
    std::unique_ptr<QObject> glyphMapper(glyphMap.create());
    if (!glyphMapper) {
        qCritical() << glyphMap.errors();
        return 23;
    }
    QStringList networkStates;
    for (const QString &strength : {QStringLiteral("0"), QStringLiteral("20"), QStringLiteral("40"),
             QStringLiteral("60"), QStringLiteral("80"), QStringLiteral("100")}) {
        for (const QString &badge : {QString(), QStringLiteral("-locked"), QStringLiteral("-limited")}) {
            networkStates << QStringLiteral("network-wireless-") + strength + badge + QStringLiteral("-symbolic")
                          << QStringLiteral("network-mobile-") + strength + QStringLiteral("-lte") + badge
                          << QStringLiteral("network-mobile-") + strength + badge;
        }
    }
    networkStates << QStringLiteral("network-wireless-connected-00") << QStringLiteral("network-wired-activated")
                  << QStringLiteral("network-wired-activated-locked-symbolic")
                  << QStringLiteral("network-wired-activated-limited") << QStringLiteral("network-wired")
                  << QStringLiteral("network-wired-disconnected") << QStringLiteral("network-wireless-disconnected")
                  << QStringLiteral("network-wireless-off") << QStringLiteral("network-wireless-hotspot")
                  << QStringLiteral("network-wireless-bluetooth-symbolic") << QStringLiteral("network-mobile-available")
                  << QStringLiteral("network-modem") << QStringLiteral("network-vpn")
                  << QStringLiteral("network-flightmode-on-symbolic") << QStringLiteral("network-unavailable")
                  << QStringLiteral("network-something-new");
    const QStringList appletStates{
        QStringLiteral("network-bluetooth-activated-symbolic"), QStringLiteral("network-bluetooth-inactive-symbolic"),
        QStringLiteral("network-bluetooth-symbolic"), QStringLiteral("audio-volume-muted-symbolic"),
        QStringLiteral("audio-volume-low-symbolic"), QStringLiteral("audio-volume-medium-symbolic"),
        QStringLiteral("audio-volume-high-symbolic"), QStringLiteral("audio-volume-high-danger-symbolic-rtl"),
        QStringLiteral("microphone-sensitivity-muted-symbolic"), QStringLiteral("audio-input-microphone-symbolic"),
        QStringLiteral("battery-full-symbolic"), QStringLiteral("battery-040-charging"), QStringLiteral("battery-010"),
        QStringLiteral("battery-100-charging-symbolic"), QStringLiteral("battery-060"), QStringLiteral("battery-missing"),
        QStringLiteral("speedometer"), QStringLiteral("redshift-status-on"), QStringLiteral("klipper-symbolic"),
        QStringLiteral("device-notifier-symbolic"), QStringLiteral("drive-removable-media-usb-pendrive"),
        QStringLiteral("printer-symbolic"), QStringLiteral("notification-active"), QStringLiteral("network-wireless-60")
    };
    const auto everyGlyphShipped = [&](const QStringList &names, bool network) {
        QVariant result;
        QMetaObject::invokeMethod(glyphMapper.get(), "check", Q_RETURN_ARG(QVariant, result),
            Q_ARG(QVariant, QVariant(names)), Q_ARG(QVariant, network));
        const QStringList glyphNames = result.toStringList();
        for (qsizetype i = 0; i < names.size(); ++i) {
            const QString glyph = i < glyphNames.size() ? glyphNames.at(i) : QString();
            if (network && glyph == QStringLiteral("suite-ethernet")) continue;
            if (glyph.isEmpty() || !QFile::exists(QStringLiteral(":/qt/qml/plasma/applet/co/goodinput/temperance/")
                    + glyph + QStringLiteral(".svg"))) {
                qCritical() << "No shipped glyph for" << names.at(i) << glyph;
                return false;
            }
        }
        return true;
    };
    if (!everyGlyphShipped(networkStates, true) || !everyGlyphShipped(appletStates, false))
        return 23;
    QVariant appGlyph;
    QMetaObject::invokeMethod(glyphMapper.get(), "check", Q_RETURN_ARG(QVariant, appGlyph),
        Q_ARG(QVariant, QVariant(QStringList{QStringLiteral("steam"), QStringLiteral("discord-tray")})),
        Q_ARG(QVariant, false));
    if (appGlyph.toStringList() != QStringList{QString(), QString()}) {
        qCritical() << "An application's own tray icon must keep its own picture" << appGlyph;
        return 23;
    }
    QVariant lockGlyph;
    QMetaObject::invokeMethod(glyphMapper.get(), "check", Q_RETURN_ARG(QVariant, lockGlyph),
        Q_ARG(QVariant, QVariant(QStringList{QStringLiteral("network-wireless-100-symbolic"),
            QStringLiteral("network-wireless-100-locked-symbolic"),
            QStringLiteral("network-wireless-20-limited")})),
        Q_ARG(QVariant, true));
    // Plasma's lock means a VPN is up, never a passworded network.
    if (lockGlyph.toStringList() != QStringList{QStringLiteral("nm-signal-100-symbolic"),
            QStringLiteral("nm-signal-100-secure-symbolic"), QStringLiteral("network-wireless-no-route-symbolic")}) {
        qCritical() << "Network badges do not follow Plasma's VPN and limited states" << lockGlyph;
        return 23;
    }
    // Stand-ins for a dark and a light Plasma style. The software renderer
    // the test runs on draws no layer effects, so the light glyph's recolour
    // is checked by its settings and the drawing by the dark one.
    const auto makeTheme = [&](const QString &ground, const QString &text) {
        QQmlComponent themeComponent(&engine);
        themeComponent.setData(QStringLiteral("import QtQuick\nQtObject { property color backgroundColor: \"%1\"; "
            "property color textColor: \"%2\"; property color negativeTextColor: \"#DA4453\" }")
            .arg(ground, text).toUtf8(), QUrl());
        return std::unique_ptr<QObject>(themeComponent.create());
    };
    const auto darkTheme = makeTheme(QStringLiteral("#141414"), QStringLiteral("#E0E0E0"));
    const auto lightTheme = makeTheme(QStringLiteral("#F2F2F2"), QStringLiteral("#102729"));
    if (!darkTheme || !lightTheme) {
        qCritical() << "Could not make the stand-in themes";
        return 18;
    }
    for (const QString &glyph : glyphs) {
        std::unique_ptr<QObject> icon(iconComponent.createWithInitialProperties(
            {{QStringLiteral("glyph"), glyph}}));
        auto *item = qobject_cast<QQuickItem *>(icon.get());
        if (!item || !item->isVisible() || item->implicitWidth() < 18
            || item->implicitHeight() < 18 || item->width() < 18 || item->height() < 18) {
            qCritical() << "Zero-size or invisible SuiteIcon:" << glyph;
            return 15;
        }
        item->setParentItem(iconWindow.contentItem());
        auto *imageItem = item->findChild<QQuickItem *>(QStringLiteral("suiteIconImage"));
        auto *tone = item->findChild<QObject *>(QStringLiteral("temperanceStatusColors"),
            Qt::FindDirectChildrenOnly);
        if (!imageItem || !tone) {
            qCritical() << "SuiteIcon lost its image or its colours:" << glyph;
            return 16;
        }
        tone->setProperty("theme", QVariant::fromValue(lightTheme.get()));
        auto *layer = imageItem->property("layer").value<QObject *>();
        if (!layer || !layer->property("enabled").toBool()
            || item->property("ink").value<QColor>() != QColor(QStringLiteral("#102729"))) {
            qCritical() << "SuiteIcon is not recoloured to a light style's text:" << glyph;
            return 17;
        }
        tone->setProperty("theme", QVariant::fromValue(darkTheme.get()));
        if (layer->property("enabled").toBool()) {
            qCritical() << "SuiteIcon is recoloured on a dark style:" << glyph;
            return 17;
        }
        QElapsedTimer loadTimer;
        loadTimer.start();
        while (imageItem && imageItem->property("status").toInt() != 1
               && imageItem->property("status").toInt() != 3
               && loadTimer.elapsed() < 3000) {
            app.processEvents(QEventLoop::AllEvents, 20);
        }
        if (!imageItem || !imageItem->isVisible()
            || imageItem->property("status").toInt() != 1) {
            qCritical() << "SuiteIcon source did not become visible:" << glyph
                        << (imageItem ? imageItem->property("status") : QVariant());
            return 16;
        }
        const auto grab = item->grabToImage(QSize(40, 40));
        QEventLoop grabLoop;
        QTimer grabTimeout;
        grabTimeout.setSingleShot(true);
        QObject::connect(grab.data(), &QQuickItemGrabResult::ready,
            &grabLoop, &QEventLoop::quit);
        QObject::connect(&grabTimeout, &QTimer::timeout,
            &grabLoop, &QEventLoop::quit);
        grabTimeout.start(3000);
        grabLoop.exec();
        const QImage rendered = grab->image();
        bool hasVisibleInk = false;
        for (int y = 0; y < rendered.height() && !hasVisibleInk; ++y) {
            for (int x = 0; x < rendered.width(); ++x) {
                const QColor pixel = rendered.pixelColor(x, y);
                if (pixel.alpha() > 32 && pixel.red() > 180
                    && pixel.green() > 180 && pixel.blue() > 180) {
                    hasVisibleInk = true;
                    break;
                }
            }
        }
        if (!hasVisibleInk) {
            qCritical() << "SuiteIcon rendered without visible Ghost White ink:" << glyph;
            return 17;
        }
        item->setParentItem(nullptr);
    }
    std::unique_ptr<QObject> insetIcon(iconComponent.createWithInitialProperties({
        {QStringLiteral("glyph"), QStringLiteral("power")}, {QStringLiteral("glyphInset"), 2.0}
    }));
    auto *insetItem = qobject_cast<QQuickItem *>(insetIcon.get());
    if (!insetItem) return 20;
    insetItem->setWidth(18);
    insetItem->setHeight(18);
    insetItem->setParentItem(iconWindow.contentItem());
    app.processEvents();
    auto *insetImage = insetItem->findChild<QQuickItem *>(QStringLiteral("suiteIconImage"));
    if (!insetImage || insetImage->width() != 14 || insetImage->height() != 14
        || insetImage->x() != 2 || insetImage->y() != 2) {
        qCritical() << "Session glyph inset did not reduce the rendered image to 14 px";
        return 20;
    }
    insetItem->setParentItem(nullptr);
    // The level reading: quiet at start and while it only reads the volume
    // again, shown for a change, never while not asked to watch or while
    // Control Center is open.
    {
        QQmlComponent audioComponent(&engine);
        audioComponent.setData(QByteArrayLiteral("import QtQuick\nQtObject { property int percent: 40; "
            "property bool muted: false; property bool available: true; signal volumeChanged(); "
            "property bool microphoneMuted: false; property bool microphoneAvailable: true; "
            "signal microphoneChanged() }"), QUrl());
        std::unique_ptr<QObject> audio(audioComponent.create());
        QQmlComponent readingComponent(&engine,
            QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/co/goodinput/temperance/LevelReading.qml")),
            QQmlComponent::PreferSynchronous);
        std::unique_ptr<QObject> reading(audio ? readingComponent.createWithInitialProperties({
            {QStringLiteral("audio"), QVariant::fromValue(audio.get())},
            {QStringLiteral("watching"), true}, {QStringLiteral("motionEnabled"), false}}) : nullptr);
        if (!reading) {
            qCritical() << readingComponent.errors();
            return 22;
        }
        const auto kind = [&] { return reading->property("kind").toString(); };
        const auto change = [&](int percent) {
            audio->setProperty("percent", percent);
            QMetaObject::invokeMethod(audio.get(), "volumeChanged");
            app.processEvents();
        };
        change(40);
        if (!kind().isEmpty()) {
            qCritical() << "The level reading shows a volume read again";
            return 22;
        }
        change(55);
        if (kind() != QLatin1String("volume") || qAbs(reading->property("level").toReal() - 0.55) > 0.001
            || reading->property("room").toReal() <= 0) {
            qCritical() << "The level reading does not show a change of volume" << kind();
            return 22;
        }
        reading->setProperty("kind", QString());
        reading->setProperty("quiet", true);
        change(60);
        if (!kind().isEmpty()) {
            qCritical() << "The level reading speaks over Control Center";
            return 22;
        }
        reading->setProperty("quiet", false);
        const auto mute = [&](bool muted) {
            audio->setProperty("microphoneMuted", muted);
            QMetaObject::invokeMethod(audio.get(), "microphoneChanged");
            app.processEvents();
        };
        mute(false);
        if (!kind().isEmpty()) {
            qCritical() << "The level reading shows a microphone read again";
            return 22;
        }
        mute(true);
        if (kind() != QLatin1String("microphone") || reading->property("room").toReal() <= 0) {
            qCritical() << "The level reading does not show the microphone muted" << kind();
            return 22;
        }
        reading->setProperty("kind", QString());
        reading->setProperty("watching", false);
        change(70);
        if (!kind().isEmpty()) {
            qCritical() << "The level reading shows a change it was not asked to watch";
            return 22;
        }
    }
    qInfo("Built Temperance plugin and nested QML components loaded successfully");
    return 0;
}
