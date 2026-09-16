// SPDX-License-Identifier: GPL-2.0-or-later
#include <KConfigGroup>
#include <KPackage/Package>
#include <KPackage/PackageLoader>
#include <LayerShellQt/Window>
#include <Plasma/Applet>
#include <Plasma/Containment>
#include <Plasma/Corona>
#include <PlasmaQuick/AppletQuickItem>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFileInfo>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickWindow>
#include <QScreen>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class TickerCorona : public Plasma::Corona {
public:
  QRect screenGeometry(int) const override {
    return QGuiApplication::screens()
        .value(qEnvironmentVariableIntValue("ITASCA_TEST_SCREEN"),
               QGuiApplication::primaryScreen())
        ->geometry();
  }
};
class TickerPanelTest : public QObject {
  Q_OBJECT
  static QQuickItem *findItem(QQuickItem *item, const QString &name) {
    if (item->objectName() == name)
      return item;
    for (auto *child : item->childItems())
      if (auto *found = findItem(child, name))
        return found;
    return nullptr;
  }
  static void hint(QQuickItem *item, const char *name, const QVariant &value) {
    QVERIFY(QQmlProperty::write(item, QString::fromLatin1(name), value,
                                qmlContext(item)));
  }
private Q_SLOTS:
  void livePanel() {
    for (const auto &path : qEnvironmentVariable("QT_PLUGIN_PATH")
                                .split(QLatin1Char(':'), Qt::SkipEmptyParts))
      QCoreApplication::addLibraryPath(path);
    TickerCorona corona;
    auto shell = KPackage::PackageLoader::self()->loadPackage(
        QStringLiteral("Plasma/Shell"));
    shell.setPath(QStringLiteral("org.kde.plasma.desktop"));
    corona.setKPackage(shell);
    auto *panel = corona.createContainment(QStringLiteral("org.kde.panel"));
    QVERIFY(panel);
    panel->setFormFactor(Plasma::Types::Horizontal);
    panel->setLocation(Plasma::Types::BottomEdge);
    auto *face = PlasmaQuick::AppletQuickItem::itemForApplet(panel);
    QVERIFY(face);
    QQuickWindow window;
    window.setFlags(Qt::FramelessWindowHint);
    window.setScreen(QGuiApplication::screens().value(
        qEnvironmentVariableIntValue("ITASCA_TEST_SCREEN"),
        QGuiApplication::primaryScreen()));
    const auto screen = window.screen()->geometry();
    auto *surface = LayerShellQt::Window::get(&window);
    surface->setScreen(window.screen());
    surface->setScope(QStringLiteral("itasca-private-center-fixture"));
    surface->setLayer(LayerShellQt::Window::LayerTop);
    surface->setAnchors(
        LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop) |
        LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight);
    surface->setDesiredSize({screen.width(), 64});
    surface->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);
    surface->setExclusiveZone(0);
    window.setGeometry(screen.x(), screen.y(), screen.width(), 64);
    face->setParentItem(window.contentItem());
    face->setSize({qreal(screen.width()), 64});
    const auto add = [&](const char *id) {
      auto *a = panel->createApplet(QString::fromLatin1(id));
      if (a && QString::fromLatin1(id).contains(QStringLiteral("temperance"))) {
        a->config().writeEntry("extraItems", QStringList{});
        a->config().writeEntry("showWeather", false);
      }
      return a;
    };
    auto *left = add("studio.warbler.tettegouche");
    QVERIFY(left);
    QCOMPARE(
        QFileInfo(left->pluginMetaData().fileName()).canonicalFilePath(),
        QFileInfo(QString::fromLocal8Bit(qgetenv("ITASCA_TEST_TETTE_PLUGIN")))
            .canonicalFilePath());
    auto *spacer = add("org.kde.plasma.panelspacer");
    QVERIFY(spacer);
    auto *spacerFace = PlasmaQuick::AppletQuickItem::itemForApplet(spacer);
    hint(spacerFace, "plasmoid.configuration.expanding", true);
    auto *leftSpacerFace = spacerFace;
    auto *tasks = add("org.kde.plasma.icontasks");
    QVERIFY(tasks);
    spacer = add("org.kde.plasma.panelspacer");
    QVERIFY(spacer);
    spacerFace = PlasmaQuick::AppletQuickItem::itemForApplet(spacer);
    hint(spacerFace, "plasmoid.configuration.expanding", true);
    auto *right = add("studio.warbler.temperance");
    QVERIFY(right);
    QCOMPARE(QFileInfo(right->pluginMetaData().fileName()).canonicalFilePath(),
             QFileInfo(QString::fromLocal8Bit(
                           qgetenv("ITASCA_TEST_TEMPERANCE_PLUGIN")))
                 .canonicalFilePath());
    auto *clock = add("org.kde.plasma.digitalclock");
    QVERIFY(clock);
    auto *taskFace = PlasmaQuick::AppletQuickItem::itemForApplet(tasks);
    auto *leftFace = PlasmaQuick::AppletQuickItem::itemForApplet(left);
    auto *rightFace = PlasmaQuick::AppletQuickItem::itemForApplet(right);
    auto *clockFace = PlasmaQuick::AppletQuickItem::itemForApplet(clock);
    QVERIFY(taskFace && leftFace && rightFace && clockFace);
    // Deterministic task demand; actual stock panel, clock, spacers and
    // both production suite applets still own every layout step.
    hint(taskFace, "Layout.minimumWidth", 54.);
    hint(taskFace, "Layout.fillWidth", false);
    window.show();
    QTest::qWait(1000);
    // Plasma positions the AppletContainer ancestor, not the applet root.
    // A parent-only move must invalidate the scene-space width measurement.
    auto *taskContainer = taskFace->parentItem();
    QVERIFY(taskContainer && taskContainer != face);
    QSignalSpy geometryChanged(right, SIGNAL(panelGeometryChanged()));
    QSignalSpy childX(taskFace, &QQuickItem::xChanged);
    QSignalSpy childWidth(taskFace, &QQuickItem::widthChanged);
    const qreal containerX = taskContainer->x();
    taskContainer->setX(containerX - 40);
    QCOMPARE(childX.count(), 0);
    QCOMPARE(childWidth.count(), 0);
    QVERIFY2(geometryChanged.count() > 0,
             "Parent-only movement left Temperance's scene geometry stale");
    qInfo() << "T1_PARENT_MOVE" << "notifications" << geometryChanged.count()
            << "childX" << childX.count() << "childWidth" << childWidth.count();
    taskContainer->setX(containerX);
    QTest::qWait(500);
    QVERIFY(rightFace->setProperty("demoNotificationVisible", true));
    QVERIFY(QMetaObject::invokeMethod(rightFace, "revealNotificationText"));
    qreal boundaryError = 0, surfaceError = 0;
    for (bool adaptive : {true, false}) {
      hint(rightFace, "plasmoid.configuration.adaptiveWidth", adaptive);
      for (qreal demand : {108., 216., 432., 216.}) {
        hint(taskFace, "Layout.preferredWidth", demand);
        hint(taskFace, "Layout.maximumWidth", demand);
        QTest::qWait(500);
        auto *ticker =
            findItem(rightFace, QStringLiteral("temperance-ticker-content"));
        auto *controls =
            findItem(rightFace, QStringLiteral("temperance-ticker-controls"));
        auto *surface =
            findItem(rightFace, QStringLiteral("temperance-compact-surface"));
        QVERIFY(ticker && controls && surface);
        const qreal measured =
            rightFace->property("responsiveMeasuredWidth").toReal();
        const qreal available =
            rightFace->mapToGlobal({rightFace->width(), 0}).x() -
            taskFace->mapToGlobal({taskFace->width(), 0}).x() - 6;
        qInfo() << "T1_BOUNDARY" << screen << adaptive << demand << "ROOT"
                << rightFace->width() << "MEASURED" << measured << "AVAILABLE"
                << available << "SURFACE" << surface->width() << "TICKER"
                << ticker->width() << "ARROWS" << controls->width() << "SPACERS"
                << leftSpacerFace->width() << spacerFace->width() << "TASK"
                << taskFace->mapToGlobal({0, 0}) << taskFace->width() << "TETTE"
                << leftFace->width();
        auto capture = face->grabToImage();
        QVERIFY(capture);
        QSignalSpy ready(capture.get(), &QQuickItemGrabResult::ready);
        QVERIFY(ready.wait(3000));
        QVERIFY(capture->image().save(
            QString::fromLocal8Bit(qgetenv("XDG_CACHE_HOME")) +
            QStringLiteral("/panel.png")));
        QVERIFY(rightFace->setProperty("demoNotificationVisible", true));
        for (bool revealed : {false, true}) {
          QVERIFY(controls->setProperty("revealed", revealed));
          QVERIFY(
              QMetaObject::invokeMethod(rightFace, "revealNotificationText"));
          QTest::qWait(350);
          QVERIFY(ticker->width() > controls->width());
          QVERIFY(ticker->clip());
          QVERIFY(ticker->mapToGlobal({0, 0}).x() >=
                  taskFace->mapToGlobal({taskFace->width(), 0}).x());
          const qreal used =
              surface->width() - ticker->width() - controls->width();
          qInfo() << "T1_DISCLOSURE" << adaptive << demand << revealed
                  << ticker->width() << controls->width() << used;
          // Fixed status controls consume the same span in both arrow states.
          QVERIFY(used > 0 && used < 160);
          auto ink = ticker->grabToImage();
          QVERIFY(ink);
          QSignalSpy inkReady(ink.get(), &QQuickItemGrabResult::ready);
          QVERIFY(inkReady.wait(3000));
          int pixels = 0;
          for (int y = 0; y < ink->image().height(); ++y)
            for (int x = 0; x < ink->image().width(); ++x)
              pixels += qAlpha(ink->image().pixel(x, y)) > 0;
          QVERIFY(pixels > 50);
          qInfo() << "T1_INK" << pixels << ink->image().size();
        }
        if (adaptive) {
          boundaryError = std::max(
              boundaryError,
              std::abs(
                  measured -
                  std::max(
                      available,
                      rightFace->property("responsiveMinimumWidth").toReal())));
          surfaceError =
              std::max(surfaceError, std::abs(surface->width() - measured));
        } else
          QCOMPARE(surface->width(),
                   rightFace->property("compactWidth").toReal());
        QVERIFY(ticker->mapToGlobal({0, 0}).x() >=
                taskFace->mapToGlobal({taskFace->width(), 0}).x());
      }
    }
    hint(rightFace, "plasmoid.configuration.adaptiveWidth", true);
    hint(taskFace, "Layout.preferredWidth", 216.);
    hint(taskFace, "Layout.maximumWidth", 216.);
    auto message = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("Notify"));
    message.setArguments(
        {QStringLiteral("T1 fixture"), uint(0), QString(),
         QStringLiteral("Long real notification"),
         QStringLiteral("Real body text remains visible across the complete "
                        "assigned ticker region. ")
             .repeated(8),
         QStringList{}, QVariantMap{}, 0});
    QDBusPendingCallWatcher notification(
        QDBusConnection::sessionBus().asyncCall(message));
    QSignalSpy notified(&notification, &QDBusPendingCallWatcher::finished);
    QVERIFY(notified.wait(3000));
    QDBusPendingReply<uint> reply = notification;
    QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
    QTRY_VERIFY(rightFace->property("lastLiveNotificationCount").toInt() > 0);
    auto *controls =
        findItem(rightFace, QStringLiteral("temperance-ticker-controls"));
    auto *ticker =
        findItem(rightFace, QStringLiteral("temperance-ticker-content"));
    QVERIFY(controls->setProperty("revealed", true));
    QVERIFY(QMetaObject::invokeMethod(rightFace, "revealNotificationText"));
    QTest::qWait(600);
    auto *real = findItem(rightFace, QStringLiteral("temperance-live-ticker"));
    QVERIFY(real);
    auto *label = findItem(real, QStringLiteral("temperance-ticker-label"));
    QVERIFY(label);
    const qreal initialX = label->x();
    if (rightFace->property("motionEnabled").toBool())
      QTRY_VERIFY_WITH_TIMEOUT(label->x() < initialX - 2, 4000);
    QVERIFY(ticker->width() > controls->width());
    QVERIFY(ticker->mapToGlobal({0, 0}).x() >=
            taskFace->mapToGlobal({taskFace->width(), 0}).x());
    qInfo() << "T1_REAL" << reply.value() << ticker->width()
            << controls->width() << label->x() << "DPR"
            << window.devicePixelRatio();
    face->setParentItem(nullptr);
    qInfo() << "T1_ERRORS" << boundaryError << surfaceError;
    QVERIFY(boundaryError <= 2.);
    // The root's actual allocation, not its preferred hint, owns the rail.
    // Native layout gaps can leave a few pixels below the requested width.
  }
};
QTEST_MAIN(TickerPanelTest)
#include "TickerPanelTest.moc"
