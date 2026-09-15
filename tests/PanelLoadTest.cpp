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
#include <PlasmaQuick/AppletQuickItem>
#include <memory>

class TestCorona : public Plasma::Corona {
public:
    QRect screenGeometry(int) const override { return QRect(0, 0, 1280, 800); }
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
    auto *applet = panel->createApplet(QStringLiteral("studio.warbler.temperance"));
    if (!applet || applet->failedToLaunch()) return 2;
    // Test Temperance itself, not embedded third-party tray applets which
    // may require real display-server services (for example Klipper).
    applet->config().writeEntry("extraItems", QStringList{});
    const QString expected = QString::fromLocal8Bit(qgetenv("QT_PLUGIN_PATH"))
        + QStringLiteral("/plasma/applets/studio.warbler.temperance.so");
    if (QFileInfo(applet->pluginMetaData().fileName()).canonicalFilePath()
        != QFileInfo(expected).canonicalFilePath()) return 3;
    // Load the complete packaged component graph without instantiating
    // embedded tray clients (Klipper needs a real display server).
    // Unlike qmlcachegen, this rejects assignments to read-only properties
    // in nested components before the package reaches the live panel.
    QQmlEngine engine;
    QFile source(QStringLiteral(":/qt/qml/plasma/applet/studio/warbler/temperance/main.qml"));
    if (!source.open(QIODevice::ReadOnly)) return 6;
    const QString qml = QString::fromUtf8(source.readAll());
    if (!qml.contains(QStringLiteral("Plasmoid.watchPanelGeometry(root)"))
        || !qml.contains(QStringLiteral("function onPanelGeometryChanged()"))) return 13;
    if (!qml.contains(QStringLiteral("nearest real panel item on"))
        || !qml.contains(QStringLiteral("its left to this applet's fixed right edge"))) return 14;
    QString navigationFunctions;
    for (const auto &name : {"activateAppletById", "openSurface"}) {
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
    )JS") + navigationFunctions + QStringLiteral(R"JS(
            openSurface('tray');
            if (!systemTrayState.expanded) throw 'open tray';
            openSurface('tray');
            if (systemTrayState.expanded) throw 'close tray';
            openSurface('notifications'); openSurface('control');
            if (!systemTrayState.expanded || systemTrayState.page !== 'control') throw 'switch';
            activateAppletById('weather'); activateAppletById('weather');
            if (systemTrayState.expanded) throw 'close weather';
            activateAppletById('weather'); openSurface('tray');
            if (!systemTrayState.expanded || systemTrayState.activeApplet) throw 'weather to tray';
            return true;
        })()
    )JS"));
    if (navigationTest.isError() || !navigationTest.toBool()) {
        qCritical() << "Panel navigation regression:" << navigationTest.toString();
        return 12;
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
        QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/studio/warbler/temperance/main.qml")),
        QQmlComponent::PreferSynchronous);
    if (!component.isReady()) {
        qCritical() << component.errors();
        return 4;
    }
    QQmlComponent iconComponent(&engine,
        QUrl(QStringLiteral("qrc:/qt/qml/plasma/applet/studio/warbler/temperance/SuiteIcon.qml")),
        QQmlComponent::PreferSynchronous);
    if (!iconComponent.isReady()) {
        qCritical() << iconComponent.errors();
        return 14;
    }
    QQuickWindow iconWindow;
    iconWindow.setGeometry(0, 0, 64, 64);
    iconWindow.setColor(Qt::transparent);
    iconWindow.show();
    const QStringList glyphs{
        QStringLiteral("ellipsis"), QStringLiteral("settings"), QStringLiteral("plus"),
        QStringLiteral("pin"), QStringLiteral("trash-2"), QStringLiteral("chevron-up"),
        QStringLiteral("chevron-down"), QStringLiteral("log-out"), QStringLiteral("rotate-cw"),
        QStringLiteral("power"), QStringLiteral("sliders-horizontal"), QStringLiteral("sun"),
        QStringLiteral("gauge")
    };
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
    qInfo("Built Temperance plugin and nested QML components loaded successfully");
    return 0;
}
