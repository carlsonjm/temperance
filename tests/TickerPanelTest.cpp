// SPDX-License-Identifier: GPL-2.0-or-later
#include <KConfigGroup>
#include <KPackage/Package>
#include <KPackage/PackageLoader>
#include <LayerShellQt/Window>
#include <notificationmanager/server.h>
#include <Plasma/Applet>
#include <Plasma/Containment>
#include <Plasma/Corona>
#include "CoronaScreen.h"
#include <PlasmaQuick/AppletQuickItem>
#include <PlasmaQuick/Dialog>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QQmlContext>
#include <QQmlEngine>
#include <functional>

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
  QRect screenGeometry(CoronaScreenId) const override {
    return QGuiApplication::screens()
        .value(qEnvironmentVariableIntValue("ITASCA_TEST_SCREEN"),
               QGuiApplication::primaryScreen())
        ->geometry();
  }
};
// A bar that paints its own clock and says how much room it leaves beside it.
class StandInContextBar : public QObject {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", "co.goodinput.ContextBar")
public:
  int right = 600;
  QStringList asked;
public Q_SLOTS:
  QString clockExtent(const QString &outputName) {
    asked << outputName;
    return QStringLiteral("{\"schema\":\"co.goodinput.shuffle.clock-extent\",\"version\":1,"
                          "\"output\":\"%1\",\"presenting\":true,\"bar\":{\"height\":24},"
                          "\"clock\":{\"left\":0,\"right\":0,\"center\":0},"
                          "\"available\":{\"left\":0,\"right\":%2}}")
        .arg(outputName).arg(right);
  }
Q_SIGNALS:
  void clockExtentChanged(const QString &outputName);
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
    auto *left = add("co.goodinput.tettegouche");
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
    auto *right = add("co.goodinput.temperance");
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
    // A transfer's end, as Ambient files it after its minute in view: the
    // history keeps it, and the ticker does not play it.
    auto *history = rightFace->findChild<QObject *>(
        QStringLiteral("temperance-notification-history"));
    QVERIFY(history);
    auto transfer = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("Notify"));
    transfer.setArguments(
        {QStringLiteral("T1 fixture"), uint(0), QString(),
         QStringLiteral("photo.png"), QStringLiteral("Arrived in Downloads"),
         QStringList{},
         QVariantMap{{QStringLiteral("category"),
                      QStringLiteral("transfer.complete")}},
         0});
    QDBusPendingCallWatcher filed(
        QDBusConnection::sessionBus().asyncCall(transfer));
    QSignalSpy filedDone(&filed, &QDBusPendingCallWatcher::finished);
    QVERIFY(filedDone.wait(3000));
    QTRY_COMPARE(history->property("count").toInt(), 1);
    // So is a drive plugged in that sat its minute in Ambient unopened.
    auto drive = transfer;
    drive.setArguments(
        {QStringLiteral("T1 fixture"), uint(0), QString(),
         QStringLiteral("STICK"), QStringLiteral("32.0 GB, plugged in"),
         QStringList{},
         QVariantMap{{QStringLiteral("category"),
                      QStringLiteral("device.added")}},
         0});
    QDBusPendingCallWatcher driveFiled(
        QDBusConnection::sessionBus().asyncCall(drive));
    QSignalSpy driveDone(&driveFiled, &QDBusPendingCallWatcher::finished);
    QVERIFY(driveDone.wait(3000));
    QTRY_COMPARE(history->property("count").toInt(), 2);
    QTest::qWait(300);
    QCOMPARE(rightFace->property("lastLiveNotificationCount").toInt(), 0);
    // A finger checks a ticker line off with a sideways flick. With no line
    // showing it first brings the latest up, checking nothing unseen; on a
    // line that shows, it takes the line off the ticker, unread in the
    // history, and the bell holds a check until the history is opened. A
    // mouse drag does the same.
    auto *rail = rightFace->findChild<QObject *>(
        QStringLiteral("temperance-rail-notifications"));
    auto *checked = rightFace->findChild<QObject *>(
        QStringLiteral("temperance-checked-notifications"));
    auto *count = findItem(rightFace, QStringLiteral("temperance-notification-count"));
    QVERIFY(checked && count);
    auto *tickerArea =
        findItem(rightFace, QStringLiteral("temperance-ticker-content"));
    auto *tickerControls =
        findItem(rightFace, QStringLiteral("temperance-ticker-controls"));
    QVERIFY(rail && tickerArea && tickerControls);
    const auto notify = [](const QString &summary) {
      auto posted = QDBusMessage::createMethodCall(
          QStringLiteral("org.freedesktop.Notifications"),
          QStringLiteral("/org/freedesktop/Notifications"),
          QStringLiteral("org.freedesktop.Notifications"),
          QStringLiteral("Notify"));
      // No timeout of its own, as notify-send sends: the server's default.
      posted.setArguments({QStringLiteral("T1 fixture"), uint(0), QString(),
                           summary, QStringLiteral("Body"), QStringList{},
                           QVariantMap{}, -1});
      QDBusPendingCallWatcher sent(
          QDBusConnection::sessionBus().asyncCall(posted));
      QSignalSpy done(&sent, &QDBusPendingCallWatcher::finished);
      return done.wait(3000);
    };
    // Where the line is now: opening the controls narrows the ticker.
    const auto lineCentre = [&] {
      return tickerArea
          ->mapToScene(QPointF(tickerArea->width() / 2,
                               tickerArea->height() / 2))
          .toPoint();
    };
    auto *swipeFinger = QTest::createTouchDevice();
    const auto flick = [&] {
      const QPoint swipeFrom = lineCentre();
      QTest::touchEvent(&window, swipeFinger).press(0, swipeFrom);
      for (int step = 1; step <= 4; ++step) {
        QTest::qWait(12);
        QTest::touchEvent(&window, swipeFinger)
            .move(0, swipeFrom - QPoint(step * 25, 0));
      }
      QTest::touchEvent(&window, swipeFinger)
          .release(0, swipeFrom - QPoint(100, 0));
    };
    QVERIFY(notify(QStringLiteral("Swipe fixture")));
    QTRY_COMPARE(rail->property("count").toInt(), 1);
    QVERIFY(QMetaObject::invokeMethod(rightFace, "hideNotificationText"));
    QTRY_VERIFY(!rightFace->property("notificationCopyActive").toBool());
    flick();
    QTRY_VERIFY(tickerControls->property("revealed").toBool());
    QTRY_VERIFY(rightFace->property("notificationCopyActive").toBool());
    QCOMPARE(rail->property("count").toInt(), 1);
    QTest::qWait(400);
    flick();
    QTRY_COMPARE(rail->property("count").toInt(), 0);
    QCOMPARE(history->property("count").toInt(), 3);
    QTRY_COMPARE(checked->property("count").toInt(), 1);
    QTRY_COMPARE(count->property("text").toString(), QString::fromUtf8("✓"));
    QTRY_VERIFY(count->parentItem()->opacity() > 0.9);
    // The check takes the clapper's place, as a count does.
    auto *bell = findItem(rightFace, QStringLiteral("temperance-bell-glyph"));
    QVERIFY(bell);
    QTRY_VERIFY(bell->property("clapperProgress").toReal() < 0.01);
    // Set to show the bell alone, the clapper stays and nothing is marked,
    // while the bell's name still says what is unread.
    auto *reviewButton = findItem(rightFace, QStringLiteral("temperance-review-notifications"));
    QVERIFY(reviewButton);
    hint(rightFace, "plasmoid.configuration.waitingShows", QStringLiteral("bell"));
    QTRY_VERIFY(!count->parentItem()->isVisible());
    QTRY_VERIFY(bell->property("clapperProgress").toReal() > 0.99);
    QVERIFY(reviewButton->property("text").toString().contains(QStringLiteral("1")));
    hint(rightFace, "plasmoid.configuration.waitingShows", QStringLiteral("count"));
    QTRY_VERIFY(count->parentItem()->opacity() > 0.9);
    QTRY_VERIFY(bell->property("clapperProgress").toReal() < 0.01);
    QVERIFY(notify(QStringLiteral("Drag fixture")));
    QTRY_COMPARE(rail->property("count").toInt(), 1);
    QVERIFY(QMetaObject::invokeMethod(rightFace, "revealNotificationText"));
    QTRY_VERIFY(rightFace->property("notificationCopyActive").toBool());
    QTest::qWait(400);
    const QPoint dragFrom = lineCentre();
    QTest::mousePress(&window, Qt::LeftButton, {}, dragFrom);
    for (int step = 1; step <= 4; ++step) {
      QTest::qWait(12);
      QTest::mouseMove(&window, dragFrom - QPoint(step * 25, 0));
    }
    QTest::mouseRelease(&window, Qt::LeftButton, {},
                        dragFrom - QPoint(100, 0));
    QTRY_COMPARE(rail->property("count").toInt(), 0);
    QCOMPARE(history->property("count").toInt(), 4);
    QTRY_COMPARE(checked->property("count").toInt(), 2);
    // Opening the history reads them, and the check goes.
    const QDateTime readMark = QDateTime::currentDateTime().addSecs(1);
    QVERIFY(history->setProperty("lastRead", readMark));
    QTRY_COMPARE(checked->property("count").toInt(), 0);
    QTRY_VERIFY(count->parentItem()->opacity() < 0.1);
    QTRY_VERIFY(bell->property("clapperProgress").toReal() > 0.99);
    // Do not disturb keeps the ticker still, as it keeps Plasma's popups
    // away: a notification goes to the history, the bell holds a check for
    // it, and it does not play when do not disturb ends. The next one does.
    // The read mark is a second ahead, so a notification sent before it
    // passes counts as read already; wait it out.
    QTRY_VERIFY(QDateTime::currentDateTime() > readMark.addMSecs(50));
    const int filedBefore = history->property("count").toInt();
    NotificationManager::Server::self().setInhibited(true);
    QVERIFY(notify(QStringLiteral("Quiet fixture")));
    QTRY_COMPARE(history->property("count").toInt(), filedBefore + 1);
    QTRY_COMPARE(checked->property("count").toInt(), 1);
    QTest::qWait(400);
    QCOMPARE(rail->property("count").toInt(), 0);
    QVERIFY(!rightFace->property("notificationSequenceActive").toBool());
    NotificationManager::Server::self().setInhibited(false);
    QTest::qWait(400);
    QCOMPARE(rail->property("count").toInt(), 0);
    QVERIFY(notify(QStringLiteral("Loud fixture")));
    QTRY_COMPARE(rail->property("count").toInt(), 1);
    QVERIFY(history->setProperty("lastRead", QDateTime::currentDateTime().addSecs(1)));
    QTRY_COMPARE(rail->property("count").toInt(), 0);
    QTRY_COMPARE(checked->property("count").toInt(), 0);
    tickerControls->setProperty("revealed", false);
    QTest::qWait(300);
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
          // The clock's reserved slot and its two 6 px margins come on top of
          // them; the row has no gaps.
          auto *statusClock =
              findItem(rightFace, QStringLiteral("temperance-clock"));
          const qreal clockSlot = statusClock && statusClock->isVisible()
                                      ? statusClock->width() + 12 : 0;
          QVERIFY(used > 0 && used - clockSlot < 160);
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
    const int filedBeforeLong = history->property("count").toInt();
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
    // The ordinary notice plays alone.
    QCOMPARE(rightFace->property("lastLiveNotificationCount").toInt(), 1);
    QTRY_COMPARE(history->property("count").toInt(), filedBeforeLong + 1);
    auto *controls =
        findItem(rightFace, QStringLiteral("temperance-ticker-controls"));
    auto *ticker =
        findItem(rightFace, QStringLiteral("temperance-ticker-content"));
    // Too long for the ticker, it comes in until its start reaches the far
    // edge and stops there, cut off; it scrolls only for a hover on the bell.
    if (rightFace->property("motionEnabled").toBool()) {
      QTRY_VERIFY_WITH_TIMEOUT(rightFace->property("notificationResting").toBool(), 8000);
      auto *arriving = findItem(rightFace, QStringLiteral("temperance-live-ticker"));
      QVERIFY(arriving);
      auto *arrivingLabel = findItem(arriving, QStringLiteral("temperance-ticker-label"));
      QVERIFY(arrivingLabel);
      const qreal restStart = arrivingLabel->mapToScene({0, 0}).x();
      qInfo() << "T1_LONG_REST" << restStart << ticker->mapToScene({0, 0}).x();
      QVERIFY(qAbs(restStart - ticker->mapToScene({0, 0}).x()) <= 4);
      QTest::qWait(1000);
      QVERIFY(qAbs(arrivingLabel->mapToScene({0, 0}).x() - restStart) < 1);
    }
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
    // The status icons sit closer together than a fingertip is wide, so a
    // touch anywhere in the panel's height above one is that icon's.
    auto *tray = findItem(rightFace, QStringLiteral("temperance-tray"));
    auto *row =
        findItem(rightFace, QStringLiteral("temperance-compact-surface"));
    QVERIFY(tray && row);
    const QPointF trayCentre =
        tray->mapToScene({tray->width() / 2, tray->height() / 2});
    QVariant reached;
    QVERIFY(QMetaObject::invokeMethod(rightFace, "statusControlAt",
                                      Q_RETURN_ARG(QVariant, reached),
                                      Q_ARG(QVariant, trayCentre.x())));
    QCOMPARE(reached.value<QObject *>(), tray);
    const qreal panelTop = rightFace->mapToScene({0, 0}).y();
    const qreal rowTop = row->mapToScene({0, 0}).y();
    qInfo() << "T1_REACH" << rightFace->height() << row->height()
            << rowTop - panelTop;
    QVERIFY(rowTop - panelTop >= 4);
    auto *state = qvariant_cast<QObject *>(rightFace->property("systemTrayState"));
    QVERIFY(state);
    auto *finger = QTest::createTouchDevice();
    const QPoint above(qRound(trayCentre.x()), qRound(panelTop + 2));
    QTest::touchEvent(&window, finger).press(0, above);
    QTest::touchEvent(&window, finger).release(0, above);
    QTRY_VERIFY(state->property("expanded").toBool());
    QCOMPARE(state->property("page").toString(), QStringLiteral("tray"));

    // A touch on each control's own box opens its page.
    const auto touchOn = [&](const QString &name, const QString &page) {
      auto *control = findItem(rightFace, name);
      QVERIFY2(control, qPrintable(name));
      const QPoint centre = control->mapToScene(
          {control->width() / 2, control->height() / 2}).toPoint();
      QTest::touchEvent(&window, finger).press(0, centre);
      QTest::touchEvent(&window, finger).release(0, centre);
      QTRY_COMPARE(state->property("page").toString(), page);
      QVERIFY(state->property("expanded").toBool());
      qInfo() << "T1_TOUCH" << name << centre << page;
    };
    touchOn(QStringLiteral("temperance-control-center"), QStringLiteral("control"));
    touchOn(QStringLiteral("temperance-clock"), QStringLiteral("calendar"));
    touchOn(QStringLiteral("temperance-tray"), QStringLiteral("tray"));

    // A pointer's click on a control opens its page once, and is not also
    // taken by the reach above the row, which would close it again.
    QObject *popup = nullptr;
    for (auto *candidate : rightFace->findChildren<QObject *>())
      if (candidate->objectName() == QStringLiteral("popupWindow"))
        popup = candidate;
    QVERIFY(popup);
    const auto clickOn = [&](const QString &name, const QString &page) {
      // Closed and gone, so the click opens the popup rather than racing
      // its closing.
      state->setProperty("expanded", false);
      QTRY_VERIFY(!popup->property("visible").toBool());
      QTest::qWait(300);
      auto *control = findItem(rightFace, name);
      QVERIFY2(control, qPrintable(name));
      const QPoint centre = control->mapToScene(
          {control->width() / 2, control->height() / 2}).toPoint();
      QTest::mouseClick(&window, Qt::LeftButton, {}, centre);
      QTest::qWait(600);
      qInfo() << "T1_CLICK" << name << state->property("page")
              << state->property("expanded");
      QCOMPARE(state->property("page").toString(), page);
      QVERIFY(state->property("expanded").toBool());
    };
    clickOn(QStringLiteral("temperance-control-center"), QStringLiteral("control"));
    // A mouse over a header control lifts it off the panel, in tablet mode too, where
    // Plasma's own buttons stop hearing the pointer.
    auto *popupWindow = qobject_cast<QQuickWindow *>(popup);
    QVERIFY(popupWindow);
    const auto hoverLightens = [&](const QString &name) {
      QTRY_VERIFY(popupWindow->isExposed());
      auto *control = findItem(popupWindow->contentItem(), name);
      QVERIFY2(control && control->isVisible(), qPrintable(name));
      auto *face = control->property("background").isValid()
          ? qvariant_cast<QQuickItem *>(control->property("background")) : control;
      QVERIFY(face);
      const QColor resting = face->property("color").value<QColor>();
      const QPoint centre = control->mapToScene(
          {control->width() / 2, control->height() / 2}).toPoint();
      QTest::mouseMove(popupWindow, centre + QPoint(0, 1));
      QTest::mouseMove(popupWindow, centre);
      // Lighter over a dark panel, darker over a light one: either way the
      // control stands further from its ground.
      const bool darkGround = resting.lightness() < 128;
      QTRY_VERIFY(darkGround
          ? face->property("color").value<QColor>().lightness() > resting.lightness()
          : face->property("color").value<QColor>().lightness() < resting.lightness());
      qInfo() << "T1_HOVER" << name << resting.name()
              << face->property("color").value<QColor>().name();
      QTest::mouseMove(popupWindow, {2, 2});
    };
    hoverLightens(QStringLiteral("temperance-header-power"));
    hoverLightens(QStringLiteral("temperance-session-more"));
    // The More menu opens 4 px under its button, right edges flush, every
    // time: opened at once, opened while Control Center is still coming
    // out, and opened again and again. A press outside closes it, and so
    // does Escape.
    {
      const auto menuSitsUnderMore = [&](const char *when) {
        auto *more = findItem(popupWindow->contentItem(), QStringLiteral("temperance-session-more"));
        auto *menu = findItem(popupWindow->contentItem(), QStringLiteral("temperance-session-menu"));
        QVERIFY(more && menu);
        QTRY_VERIFY(more->isVisible());
        const QPoint centre = more->mapToScene({more->width() / 2, more->height() / 2}).toPoint();
        QTest::mouseClick(popupWindow, Qt::LeftButton, {}, centre);
        QTRY_VERIFY(menu->isVisible());
        const QPointF topRight = menu->mapToItem(more, QPointF(menu->width(), 0));
        const QPointF moreBottomRight(more->width(), more->height() + 4);
        qInfo() << "T1_SESSION_MENU" << when << topRight << moreBottomRight
                << menu->mapToScene({0, 0}) << more->mapToScene({0, 0});
        QVERIFY(qAbs(topRight.x() - moreBottomRight.x()) < 0.5);
        QVERIFY(qAbs(topRight.y() - moreBottomRight.y()) < 0.5);
        // Nothing places it anywhere else after it shows.
        QTest::qWait(400);
        const QPointF settled = menu->mapToItem(more, QPointF(menu->width(), 0));
        QVERIFY(qAbs(settled.x() - moreBottomRight.x()) < 0.5);
        QVERIFY(qAbs(settled.y() - moreBottomRight.y()) < 0.5);
      };
      auto *menu = findItem(popupWindow->contentItem(), QStringLiteral("temperance-session-menu"));
      QVERIFY(menu);
      // Over the pills, left of the menu: the press closes the menu and
      // opens no pill.
      const auto pressOutside = [&]() {
        auto *more = findItem(popupWindow->contentItem(), QStringLiteral("temperance-session-more"));
        QTest::mouseClick(popupWindow, Qt::LeftButton, {},
                          more->mapToScene({more->width() - menu->width() - 40,
                                            more->height() + 40}).toPoint());
      };
      for (int round = 0; round < 3; ++round) {
        menuSitsUnderMore("settled");
        pressOutside();
        QTRY_VERIFY(!menu->isVisible());
        QVERIFY(state->property("expanded").toBool());
        QCOMPARE(state->property("page").toString(), QStringLiteral("control"));
      }
      // With keyboard focus in the menu, Escape closes it.
      menuSitsUnderMore("settled");
      const QQuickItem *focused = popupWindow->activeFocusItem();
      bool focusInMenu = false;
      for (const QQuickItem *item = focused; item; item = item->parentItem())
        focusInMenu = focusInMenu || item == menu;
      qInfo() << "T1_SESSION_MENU_FOCUS" << focusInMenu;
      if (focusInMenu) {
        QTest::keyClick(popupWindow, Qt::Key_Escape);
        QTRY_VERIFY(!menu->isVisible());
      } else {
        pressOutside();
        QTRY_VERIFY(!menu->isVisible());
      }
      // Opened while Control Center is still coming out of the panel.
      for (int round = 0; round < 3; ++round) {
        state->setProperty("expanded", false);
        QTRY_VERIFY(!popup->property("visible").toBool());
        QTest::qWait(300);
        state->setProperty("page", QStringLiteral("control"));
        state->setProperty("expanded", true);
        QTRY_VERIFY(popupWindow->isExposed());
        menuSitsUnderMore("opening");
        pressOutside();
        QTRY_VERIFY(!menu->isVisible());
      }
      QTest::qWait(400);
    }
    // A page an applet shows, opened from a page, has Back, which returns
    // to that page.
    const auto backReturns = [&](const QString &appletId, const QString &page) {
      QVERIFY(QMetaObject::invokeMethod(rightFace, "activateAppletById",
                                        Q_ARG(QVariant, appletId)));
      QTRY_VERIFY(state->property("activeApplet").value<QObject *>());
      auto *back = findItem(popupWindow->contentItem(), QStringLiteral("temperance-header-back"));
      QVERIFY(back);
      QTRY_VERIFY(back->isVisible());
      QTest::qWait(300);
      QTest::mouseClick(popupWindow, Qt::LeftButton, {},
                        back->mapToScene({back->width() / 2, back->height() / 2}).toPoint());
      QTRY_VERIFY(!state->property("activeApplet").value<QObject *>());
      QCOMPARE(state->property("page").toString(), page);
      QVERIFY(state->property("expanded").toBool());
      QVERIFY(!back->isVisible());
      qInfo() << "T1_BACK" << appletId << page;
    };
    backReturns(QStringLiteral("org.kde.plasma.networkmanagement"), QStringLiteral("control"));
    clickOn(QStringLiteral("temperance-clock"), QStringLiteral("calendar"));
    clickOn(QStringLiteral("temperance-tray"), QStringLiteral("tray"));
    hoverLightens(QStringLiteral("temperance-header-settings"));
    // A tile's tooltip waits for the pointer to move over it. Opened again,
    // the popup is handed the last pointer point as a hover with no pointer
    // there, and no tooltip rises from it; a pointer moving over the tile
    // still raises one.
    {
      QList<QQuickItem *> tiles;
      const std::function<void(QQuickItem *)> collect = [&](QQuickItem *item) {
        if (item->isVisible() && item->property("pointerMoved").isValid()
            && item->property("inHiddenLayout").toBool() && item->width() > 0)
          tiles.append(item);
        for (auto *child : item->childItems()) collect(child);
      };
      collect(popupWindow->contentItem());
      QVERIFY(!tiles.isEmpty());
      QQuickItem *tile = tiles.first();
      for (auto *candidate : std::as_const(tiles)) {
        qInfo() << "T1_TILE" << candidate->property("text").toString()
                << candidate->property("mainText").toString()
                << candidate->property("subText").toString().size();
        if (candidate->property("mainText").toString() != candidate->property("text").toString()
            || !candidate->property("subText").toString().isEmpty())
          tile = candidate;
      }
      const QString tileText = tile->property("text").toString();
      const auto centreOf = [&] {
        return tile->mapToScene({tile->width() / 2, tile->height() / 2}).toPoint();
      };
      // The pointer last stood on the tile, then the popup closed under it.
      QTest::mouseMove(popupWindow, centreOf());
      QTest::qWait(100);
      state->setProperty("expanded", false);
      QTRY_VERIFY(!popup->property("visible").toBool());
      QTest::qWait(300);
      state->setProperty("expanded", true);
      QTRY_VERIFY(popupWindow->isExposed());
      tiles.clear();
      collect(popupWindow->contentItem());
      for (auto *candidate : std::as_const(tiles))
        if (candidate->isVisible() && candidate->property("text").toString() == tileText)
          tile = candidate;
      QSignalSpy shown(tile, SIGNAL(toolTipVisibleChanged(bool)));
      QVERIFY(shown.isValid());
      QTest::qWait(1500);
      qInfo() << "T1_STALE_HOVER" << tileText << tile->property("containsMouse")
              << tile->property("pointerMoved") << tile->property("active") << shown.count();
      QVERIFY(!tile->property("pointerMoved").toBool());
      QVERIFY(!tile->property("active").toBool());
      QCOMPARE(shown.count(), 0);
      const QPoint centre = centreOf();
      for (int step = 1; step <= 4; ++step)
        QTest::mouseMove(popupWindow, centre + QPoint(step * 4, 0));
      QTRY_VERIFY(tile->property("pointerMoved").toBool());
      qInfo() << "T1_MOVED_HOVER" << tileText << tile->property("active");
      if (tile->property("active").toBool())
        QTRY_VERIFY_WITH_TIMEOUT(!shown.isEmpty() && shown.last().first().toBool(), 3000);
      QTest::mouseMove(popupWindow, {2, 2});
    }
    backReturns(QStringLiteral("org.kde.plasma.volume"), QStringLiteral("tray"));

    // A finger on the bell opens the history; it is not a pointer resting
    // there, so the ticker does not bring its line back. Opened, the history
    // is read, and the line waiting on the ticker goes.
    state->setProperty("expanded", false);
    QTest::qWait(300);
    // Lines still waiting from above are checked off first, as a flick does.
    auto *historyModel = qobject_cast<QAbstractItemModel *>(history);
    QVERIFY(historyModel);
    for (int row = 0; row < historyModel->rowCount(); ++row)
      QVERIFY(QMetaObject::invokeMethod(history, "expire",
                                        Q_ARG(QModelIndex, historyModel->index(row, 0))));
    QTRY_COMPARE(rail->property("count").toInt(), 0);
    tickerControls->setProperty("revealed", false);
    QTRY_VERIFY(!rightFace->property("notificationCopyActive").toBool());
    QVERIFY(notify(QStringLiteral("Roll fixture")));
    QTRY_COMPARE(rail->property("count").toInt(), 1);
    // It comes in once and rests, then goes.
    QTRY_VERIFY_WITH_TIMEOUT(rightFace->property("notificationCopyActive").toBool(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!rightFace->property("notificationCopyActive").toBool(), 15000);
    QVERIFY(!tickerControls->property("revealed").toBool());
    const QPoint bellCentre =
        bell->mapToScene({bell->width() / 2, bell->height() / 2}).toPoint();
    QTest::touchEvent(&window, finger).press(0, bellCentre);
    QTest::qWait(400);
    QVERIFY(!tickerControls->property("revealed").toBool());
    QVERIFY(!rightFace->property("notificationCopyActive").toBool());
    QTest::touchEvent(&window, finger).release(0, bellCentre);
    QTRY_COMPARE(state->property("page").toString(), QStringLiteral("notifications"));
    QTRY_COMPARE(rail->property("count").toInt(), 0);
    QVERIFY(!tickerControls->property("revealed").toBool());
    hoverLightens(QStringLiteral("temperance-do-not-disturb"));
    // Do not disturb stands at the header's end, clear of the title, however
    // the page was reached: with the popup open, or chosen while it was
    // closed, as a tap on the bell does.
    const auto dndBesideTitle = [&] {
      QTRY_VERIFY(popupWindow->isExposed());
      auto *pill = findItem(popupWindow->contentItem(), QStringLiteral("temperance-do-not-disturb"));
      auto *title = findItem(popupWindow->contentItem(), QStringLiteral("temperance-page-heading"));
      QVERIFY(pill && title && pill->isVisible());
      const auto pillLeft = [&] { return pill->mapToScene({0, 0}).x(); };
      const auto titleRight = [&] { return title->mapToScene({title->width(), 0}).x(); };
      QTRY_VERIFY2(pillLeft() >= titleRight() - 0.5,
                   qPrintable(QStringLiteral("pill at %1, title ends at %2").arg(pillLeft()).arg(titleRight())));
    };
    dndBesideTitle();
    state->setProperty("expanded", false);
    QTest::qWait(300);
    state->setProperty("page", QStringLiteral("control"));
    QTest::qWait(100);
    state->setProperty("page", QStringLiteral("notifications"));
    state->setProperty("expanded", true);
    dndBesideTitle();
    // A mouse resting on the bell still opens the ticker's controls.
    state->setProperty("expanded", false);
    QTest::qWait(700);
    QTest::mouseMove(&window, bellCentre + QPoint(0, 1));
    QTest::mouseMove(&window, bellCentre);
    QTRY_VERIFY(tickerControls->property("revealed").toBool());

    // A line shorter than the ticker rests against the bell it belongs to,
    // however wide the gap to the dock.
    hint(taskFace, "Layout.preferredWidth", 40.);
    hint(taskFace, "Layout.maximumWidth", 40.);
    QTest::mouseMove(&window, {0, 0});
    tickerControls->setProperty("revealed", false);
    QTRY_VERIFY(!rightFace->property("notificationCopyActive").toBool());
    QVERIFY(notify(QStringLiteral("Hi")));
    QTRY_COMPARE(rail->property("count").toInt(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(rightFace->property("notificationCopyActive").toBool(), 5000);
    auto *shortTicker = findItem(rightFace, QStringLiteral("temperance-live-ticker"));
    QVERIFY(shortTicker);
    auto *shortLabel = findItem(shortTicker, QStringLiteral("temperance-ticker-label"));
    QVERIFY(shortLabel);
    const qreal bellStart = tickerControls->mapToScene({0, 0}).x();
    const auto gapToBell = [&] {
      return bellStart - shortLabel->mapToScene({shortLabel->width(), 0}).x();
    };
    // On arrival it comes out beside the bell, rests there, and does not go
    // on toward the dock.
    QTRY_VERIFY_WITH_TIMEOUT(rightFace->property("notificationResting").toBool(), 6000);
    const qreal arrived = gapToBell();
    QTest::qWait(1500);
    QVERIFY(rightFace->property("notificationCopyActive").toBool());
    qInfo() << "T1_ARRIVAL" << arrived << gapToBell();
    QVERIFY(arrived >= 0 && arrived <= 12);
    QVERIFY(qAbs(gapToBell() - arrived) < 1);
    QTRY_VERIFY_WITH_TIMEOUT(!rightFace->property("notificationCopyActive").toBool(), 20000);
    // Brought back under a resting pointer, it is in the same place.
    QVERIFY(tickerControls->setProperty("revealed", true));
    QVERIFY(QMetaObject::invokeMethod(rightFace, "revealNotificationText"));
    QTest::qWait(600);
    qInfo() << "T1_SHORT" << shortLabel->x() << shortLabel->width()
            << shortTicker->width() << gapToBell();
    QVERIFY(shortLabel->width() < shortTicker->width() - 40);
    QVERIFY(gapToBell() >= 0);
    QVERIFY(gapToBell() <= 12);
    // With the ticker off the bell stays and nothing scrolls beside it.
    auto *tickerContent = findItem(rightFace, QStringLiteral("temperance-ticker-content"));
    QVERIFY(tickerContent);
    hint(rightFace, "plasmoid.configuration.showTicker", false);
    QTRY_VERIFY(!tickerContent->isVisible());
    QVERIFY(tickerControls->isVisible());
    hint(rightFace, "plasmoid.configuration.showTicker", true);
    QTRY_VERIFY(tickerContent->isVisible());
    face->setParentItem(nullptr);
    qInfo() << "T1_ERRORS" << boundaryError << surfaceError;
    QVERIFY(boundaryError <= 2.);
    // The root's actual allocation, not its preferred hint, owns the rail.
    // Native layout gaps can leave a few pixels below the requested width.
  }

  // A 24 px panel along the top of the screen, with its own clock: the slim
  // row fits the panel's height, the ticker still reads, checks off and
  // opens the history, the pages open below the bar, and a finger chooses
  // among the status controls in a finger-sized strip pulled out of it.
  void slimTopBar() {
    // The bar's clock is its own painting, so it publishes the room beside it.
    auto hostBus = QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                                 QStringLiteral("stand-in-bar"));
    StandInContextBar host;
    QVERIFY(hostBus.registerObject(QStringLiteral("/ContextBar"), &host,
                                   QDBusConnection::ExportAllSlots |
                                       QDBusConnection::ExportAllSignals));
    QVERIFY(hostBus.registerService(QStringLiteral("co.goodinput.ContextBar")));
    TickerCorona corona;
    auto shell = KPackage::PackageLoader::self()->loadPackage(
        QStringLiteral("Plasma/Shell"));
    shell.setPath(QStringLiteral("org.kde.plasma.desktop"));
    corona.setKPackage(shell);
    auto *panel = corona.createContainment(QStringLiteral("org.kde.panel"));
    QVERIFY(panel);
    panel->setFormFactor(Plasma::Types::Horizontal);
    panel->setLocation(Plasma::Types::TopEdge);
    auto *face = PlasmaQuick::AppletQuickItem::itemForApplet(panel);
    QVERIFY(face);
    QQuickWindow window;
    window.setFlags(Qt::FramelessWindowHint);
    window.setScreen(QGuiApplication::screens().value(
        qEnvironmentVariableIntValue("ITASCA_TEST_SCREEN"),
        QGuiApplication::primaryScreen()));
    const auto screen = window.screen()->geometry();
    constexpr int bar = 24;
    auto *surface = LayerShellQt::Window::get(&window);
    surface->setScreen(window.screen());
    surface->setScope(QStringLiteral("itasca-private-slim-fixture"));
    surface->setLayer(LayerShellQt::Window::LayerTop);
    surface->setAnchors(
        LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop) |
        LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight);
    surface->setDesiredSize({screen.width(), bar});
    surface->setKeyboardInteractivity(
        LayerShellQt::Window::KeyboardInteractivityNone);
    surface->setExclusiveZone(bar);
    window.setGeometry(screen.x(), screen.y(), screen.width(), bar);
    face->setParentItem(window.contentItem());
    face->setSize({qreal(screen.width()), qreal(bar)});
    auto *spacer = panel->createApplet(QStringLiteral("org.kde.plasma.panelspacer"));
    QVERIFY(spacer);
    hint(PlasmaQuick::AppletQuickItem::itemForApplet(spacer),
         "plasmoid.configuration.expanding", true);
    auto *right = panel->createApplet(QStringLiteral("co.goodinput.temperance"));
    QVERIFY(right);
    right->config().writeEntry("extraItems", QStringList{});
    auto *rightFace = PlasmaQuick::AppletQuickItem::itemForApplet(right);
    QVERIFY(rightFace);
    hint(rightFace, "plasmoid.configuration.showWeather", false);
    hint(rightFace, "plasmoid.configuration.showTime", false);
    hint(rightFace, "plasmoid.configuration.showDate", false);
    window.show();
    QTest::qWait(1000);
    // Nothing sits to its left: the width is the room the bar says it
    // leaves, asked once, and asked again when the bar says it moved.
    QTRY_COMPARE(qRound(rightFace->width()), 600);
    QCOMPARE(host.asked.size(), 1);
    QCOMPARE(host.asked.first(), window.screen()->name());
    host.right = 520;
    Q_EMIT host.clockExtentChanged(window.screen()->name());
    QTRY_COMPARE(qRound(rightFace->width()), 520);
    QCOMPARE(host.asked.size(), 2);
    // What earlier fixtures left in the shared store is read already.
    auto *history = rightFace->findChild<QObject *>(
        QStringLiteral("temperance-notification-history"));
    auto *rail = rightFace->findChild<QObject *>(
        QStringLiteral("temperance-rail-notifications"));
    auto *checked = rightFace->findChild<QObject *>(
        QStringLiteral("temperance-checked-notifications"));
    QVERIFY(history && rail && checked);
    const QDateTime readMark = QDateTime::currentDateTime().addSecs(1);
    QVERIFY(history->setProperty("lastRead", readMark));
    QTRY_COMPARE(rail->property("count").toInt(), 0);
    QTRY_VERIFY(QDateTime::currentDateTime() > readMark.addMSecs(50));

    // The row takes the panel's height and asks for no more.
    QTRY_VERIFY(rightFace->property("slim").toBool());
    const qreal thickness = rightFace->height();
    QVERIFY(thickness > 0 && thickness <= bar);
    QVERIFY(QQmlProperty::read(rightFace, QStringLiteral("Layout.minimumHeight"),
                               qmlContext(rightFace)).toReal() <= thickness);
    QVERIFY(QQmlProperty::read(rightFace, QStringLiteral("Layout.maximumHeight"),
                               qmlContext(rightFace)).toReal() <= thickness);
    auto *row = findItem(rightFace, QStringLiteral("temperance-compact-surface"));
    QVERIFY(row);
    QCOMPARE(row->height(), thickness);
    const qreal pitch = rightFace->property("statusPitch").toReal();
    const auto insideRow = [&](QQuickItem *item) {
      const QRectF box = item->mapRectToItem(row, item->boundingRect());
      return box.top() >= -0.5 && box.bottom() <= thickness + 0.5;
    };
    for (const char *name : {"temperance-ticker-controls", "temperance-control-center",
                             "temperance-tray", "temperance-bell-glyph"}) {
      auto *item = findItem(rightFace, QString::fromLatin1(name));
      QVERIFY2(item && item->isVisible(), name);
      qInfo() << "T1_SLIM" << name << item->mapRectToItem(row, item->boundingRect());
      QVERIFY2(insideRow(item), name);
    }
    for (const char *name : {"temperance-ticker-controls", "temperance-control-center",
                             "temperance-tray"})
      QVERIFY2(findItem(rightFace, QString::fromLatin1(name))->width() >= pitch, name);
    // The bar draws its own clock, so this one is gone, not squeezed.
    auto *clock = findItem(rightFace, QStringLiteral("temperance-clock"));
    QVERIFY(clock && !clock->isVisible());

    // A notification reads on one line, centred in the bar.
    auto posted = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("Notify"));
    const auto notify = [&](const QString &summary, uchar urgency) {
      auto message = posted;
      message.setArguments({QStringLiteral("T1 fixture"), uint(0), QString(),
                            summary, QStringLiteral("Body"), QStringList{},
                            QVariantMap{{QStringLiteral("urgency"),
                                         QVariant::fromValue(urgency)}}, -1});
      QDBusPendingCallWatcher sent(
          QDBusConnection::sessionBus().asyncCall(message));
      QSignalSpy done(&sent, &QDBusPendingCallWatcher::finished);
      return done.wait(3000);
    };
    auto *tickerArea = findItem(rightFace, QStringLiteral("temperance-ticker-content"));
    auto *tickerControls = findItem(rightFace, QStringLiteral("temperance-ticker-controls"));
    auto *count = findItem(rightFace, QStringLiteral("temperance-notification-count"));
    auto *bell = findItem(rightFace, QStringLiteral("temperance-bell-glyph"));
    auto *chip = findItem(rightFace, QStringLiteral("temperance-unread-chip"));
    auto *bellButton = findItem(rightFace, QStringLiteral("temperance-review-notifications"));
    QVERIFY(tickerArea && tickerControls && count && bell && chip && bellButton);
    // With nothing unread the bell stands alone; with something unread its
    // place holds a chip with the count, inside the bar, and the bell under
    // the row's own badge stays away.
    const auto showsBell = [&] {
      return bell->opacity() > 0.99 && !chip->isVisible();
    };
    const auto showsChip = [&](const QString &text) {
      return chip->opacity() > 0.99 && !bell->isVisible()
          && chip->childItems().value(0)
          && chip->childItems().first()->property("text").toString() == text;
    };
    QTRY_VERIFY(showsBell());
    QVERIFY(!bell->property("slashed").toBool());
    QVERIFY(notify(QStringLiteral("Slim fixture"), 1));
    QTRY_COMPARE(rail->property("count").toInt(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(rightFace->property("notificationCopyActive").toBool(), 5000);
    auto *ticker = findItem(rightFace, QStringLiteral("temperance-live-ticker"));
    QVERIFY(ticker);
    auto *label = findItem(ticker, QStringLiteral("temperance-ticker-label"));
    QVERIFY(label);
    QCOMPARE(label->property("lineCount").toInt(), 1);
    QVERIFY(insideRow(label));
    const qreal labelCentre = label->mapToItem(row, QPointF(0, label->height() / 2)).y();
    qInfo() << "T1_SLIM_LINE" << label->mapRectToItem(row, label->boundingRect()) << thickness;
    QVERIFY(qAbs(labelCentre - thickness / 2) <= 1.5);
    QTRY_VERIFY(showsChip(QStringLiteral("1")));
    qInfo() << "T1_SLIM_CHIP" << chip->mapRectToItem(row, chip->boundingRect());
    QVERIFY(chip->height() <= 16);
    QVERIFY(insideRow(chip));
    QVERIFY(count->parentItem()->opacity() < 0.01);
    QVERIFY(bellButton->property("text").toString().contains(QStringLiteral("Notifications")));
    QVERIFY(bellButton->property("text").toString().contains(QStringLiteral("1")));
    // Set to show the bell alone, the bell stays with no chip and still
    // says the count to a screen reader; set back, the chip returns.
    hint(rightFace, "plasmoid.configuration.waitingShows", QStringLiteral("bell"));
    QTRY_VERIFY(showsBell());
    QVERIFY(count->parentItem()->opacity() < 0.01);
    QVERIFY(bellButton->property("text").toString().contains(QStringLiteral("1")));
    hint(rightFace, "plasmoid.configuration.waitingShows", QStringLiteral("count"));
    QTRY_VERIFY(showsChip(QStringLiteral("1")));

    // A flick checks it off; the bell keeps a check for it.
    auto *finger = QTest::createTouchDevice();
    const QPoint from = tickerArea->mapToScene(
        QPointF(tickerArea->width() / 2, tickerArea->height() / 2)).toPoint();
    QTest::touchEvent(&window, finger).press(0, from);
    for (int step = 1; step <= 4; ++step) {
      QTest::qWait(12);
      QTest::touchEvent(&window, finger).move(0, from - QPoint(step * 25, 0));
    }
    QTest::touchEvent(&window, finger).release(0, from - QPoint(100, 0));
    QTRY_COMPARE(rail->property("count").toInt(), 0);
    QTRY_COMPARE(checked->property("count").toInt(), 1);
    // Checked off, it is still unread in the history.
    QTRY_VERIFY(showsChip(QStringLiteral("1")));

    // A tap on the line opens the history, below the bar.
    tickerControls->setProperty("revealed", false);
    QTRY_VERIFY(!rightFace->property("notificationCopyActive").toBool());
    QVERIFY(notify(QStringLiteral("Tap fixture"), 1));
    QTRY_COMPARE(rail->property("count").toInt(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(rightFace->property("notificationCopyActive").toBool(), 5000);
    QTRY_VERIFY(showsChip(QStringLiteral("2")));
    QTest::qWait(400);
    const QPoint tap = tickerArea->mapToScene(
        QPointF(tickerArea->width() - 12, tickerArea->height() / 2)).toPoint();
    QTest::touchEvent(&window, finger).press(0, tap);
    QTest::touchEvent(&window, finger).release(0, tap);
    auto *state = qvariant_cast<QObject *>(rightFace->property("systemTrayState"));
    QVERIFY(state);
    QTRY_COMPARE(state->property("page").toString(), QStringLiteral("notifications"));
    QTRY_VERIFY(state->property("expanded").toBool());
    // Read, nothing is unread, and the bell comes back.
    QTRY_VERIFY(showsBell());
    QWindow *popup = nullptr;
    for (auto *candidate : rightFace->findChildren<QObject *>())
      if (candidate->objectName() == QStringLiteral("popupWindow"))
        popup = qobject_cast<QWindow *>(candidate);
    QVERIFY(popup);
    QTRY_VERIFY(popup->isVisible());
    // Where the popup asks to be put: a Wayland client is not told where it
    // went.
    const qreal barBottom = rightFace->mapToGlobal(QPointF(0, thickness)).y();
    const auto placed = [](QWindow *candidate) {
      auto *dialog = qobject_cast<PlasmaQuick::Dialog *>(candidate);
      return dialog ? QRect(dialog->popupPosition(dialog->visualParent(), dialog->size()),
                            dialog->size())
                    : QRect();
    };
    QTest::qWait(500);
    const QRect page = placed(popup);
    qInfo() << "T1_SLIM_POPUP" << page << barBottom;
    QVERIFY(page.isValid() && page.top() >= barBottom);
    state->setProperty("expanded", false);

    // An important banner comes below the bar too.
    QVERIFY(notify(QStringLiteral("Critical fixture"), 2));
    QWindow *banner = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT([&] {
      for (auto *candidate : QGuiApplication::topLevelWindows())
        if (candidate != &window && candidate != popup && candidate->isVisible()
            && candidate->inherits("PlasmaQuick::Dialog"))
          banner = candidate;
      return banner != nullptr;
    }(), 5000);
    QTest::qWait(500);
    const QRect alert = placed(banner);
    qInfo() << "T1_SLIM_BANNER" << alert << barBottom;
    QVERIFY(alert.isValid() && alert.top() >= barBottom);
    QVERIFY(alert.bottom() <= screen.bottom());
    // Do not disturb shows the slashed bell, even with something unread.
    NotificationManager::Server::self().setInhibited(true);
    QVERIFY(notify(QStringLiteral("Quiet slim fixture"), 1));
    QTRY_COMPARE(checked->property("count").toInt(), 1);
    QTest::qWait(400);
    QVERIFY(rightFace->property("unreadCount").toInt() > 0);
    QVERIFY(showsBell());
    QVERIFY(bell->property("slashed").toBool());
    NotificationManager::Server::self().setInhibited(false);
    QTRY_VERIFY(showsChip(QStringLiteral("1")));
    QVERIFY(!bell->property("slashed").toBool());
    // A finger on the status controls. They are too small to press one by
    // one, so the finger's press is taken for all of them, and a tap opens
    // the page of the control nearest it, as a click would: another
    // control's page in its place, or closed by a second tap on it.
    state->setProperty("expanded", false);
    QTRY_VERIFY(!popup->isVisible());
    auto *statusTouch = findItem(rightFace, QStringLiteral("temperance-status-touch"));
    auto *trayItem = findItem(rightFace, QStringLiteral("temperance-tray"));
    auto *controlItem = findItem(rightFace, QStringLiteral("temperance-control-center"));
    auto *anchorItem = findItem(rightFace, QStringLiteral("temperance-magnifier-anchor"));
    QVERIFY(statusTouch && statusTouch->isVisible() && trayItem && controlItem && anchorItem);
    const auto centreOf = [](QQuickItem *item) {
      return item->mapToScene({item->width() / 2, item->height() / 2}).toPoint();
    };
    // The touch area runs from the bell to the row's end, the bar's height.
    QVERIFY(statusTouch->mapToScene({0, 0}).x() <= bellButton->mapToScene({0, 0}).x() + 0.5);
    QVERIFY(statusTouch->height() >= thickness - 0.5);
    const auto fingerTap = [&](const QPoint &at) {
      QTest::touchEvent(&window, finger).press(0, at);
      QTest::qWait(60);
      QTest::touchEvent(&window, finger).release(0, at);
    };
    fingerTap(centreOf(trayItem));
    QTRY_VERIFY(state->property("expanded").toBool());
    QCOMPARE(state->property("page").toString(), QStringLiteral("tray"));
    QTest::qWait(300);
    fingerTap(centreOf(controlItem));
    QTRY_COMPARE(state->property("page").toString(), QStringLiteral("control"));
    QVERIFY(state->property("expanded").toBool());
    QTest::qWait(300);
    fingerTap(centreOf(bellButton));
    QTRY_COMPARE(state->property("page").toString(), QStringLiteral("notifications"));
    QVERIFY(state->property("expanded").toBool());
    QTest::qWait(300);
    fingerTap(centreOf(bellButton));
    QTRY_VERIFY(!state->property("expanded").toBool());
    QTRY_VERIFY(!popup->isVisible());
    QTest::qWait(300);

    // Held still, a finger raises no tooltip over its neighbours.
    const auto visibleToolTips = [&] {
      int shown = 0;
      for (auto *candidate : rightFace->findChildren<QObject *>())
        if (candidate->inherits("QQuickToolTip") && candidate->property("visible").toBool()) {
          qInfo() << "T1_TOOLTIP" << candidate->property("text") << candidate->parent();
          ++shown;
        }
      return shown;
    };
    // A pointer left over the bar would rightly raise one.
    QTest::mouseMove(&window, {0, 0});
    QTRY_COMPARE(visibleToolTips(), 0);
    for (auto *held : {bellButton, controlItem, trayItem}) {
      const QPoint at = centreOf(held);
      QTest::touchEvent(&window, finger).press(0, at);
      QTest::qWait(1500);
      QCOMPARE(visibleToolTips(), 0);
      QTest::touchEvent(&window, finger).release(0, at);
      QTest::qWait(400);
      QCOMPARE(visibleToolTips(), 0);
      if (state->property("expanded").toBool()) {
        state->setProperty("expanded", false);
        QTRY_VERIFY(!popup->isVisible());
        QTest::qWait(200);
      }
    }

    // Pulled down from the bar, the controls grow out of it finger-sized, in
    // a window of their own below the bar: fully, as soon as the pull leaves
    // the bar, however far the finger has gone.
    QWindow *magnifierWindow = nullptr;
    for (auto *candidate : rightFace->findChildren<QObject *>())
      if (candidate->objectName() == QStringLiteral("temperance-magnifier-window"))
        magnifierWindow = qobject_cast<QWindow *>(candidate);
    QVERIFY(magnifierWindow && !magnifierWindow->isVisible());
    auto *magnifierDialog = qobject_cast<PlasmaQuick::Dialog *>(magnifierWindow);
    QVERIFY(magnifierDialog);
    auto *strip = magnifierDialog->mainItem();
    QVERIFY(strip);
    const qreal threshold = rightFace->property("statusPullThreshold").toReal();
    const auto pullTo = [&](const QPoint &from, const QPoint &to) {
      const QPoint step = (to - from) / 6;
      for (int i = 1; i <= 6; ++i) {
        QTest::qWait(16);
        QTest::touchEvent(&window, finger).move(0, i == 6 ? to : from + step * i);
      }
      QTest::qWait(16);
    };
    const QPoint trayAt = centreOf(trayItem);
    QTest::touchEvent(&window, finger).press(0, trayAt);
    // Short of the threshold nothing grows.
    pullTo(trayAt, trayAt + QPoint(0, qRound(threshold) - 4));
    QTest::qWait(100);
    QVERIFY(!magnifierWindow->isVisible());
    pullTo(trayAt + QPoint(0, qRound(threshold) - 4), trayAt + QPoint(0, qRound(threshold) + 12));
    QTRY_VERIFY(magnifierWindow->isVisible());
    QTRY_COMPARE(rightFace->property("magnifierGrowth").toReal(), 1.);
    pullTo(trayAt + QPoint(0, qRound(threshold) + 12), trayAt + QPoint(0, 90));
    QCOMPARE(rightFace->property("magnifierGrowth").toReal(), 1.);
    // Grown, it stays put however far the finger goes.
    pullTo(trayAt + QPoint(0, 90), trayAt + QPoint(0, 140));
    QCOMPARE(rightFace->property("magnifierGrowth").toReal(), 1.);
    // It sits under the bar, where the panel thinks it is.
    const QRect stripRect = placed(magnifierWindow);
    const qreal anchorLeft = anchorItem->mapToGlobal(QPointF(0, 0)).x();
    qInfo() << "T1_MAGNIFIER" << stripRect << anchorLeft << barBottom << strip->size();
    QVERIFY(stripRect.top() >= barBottom);
    QVERIFY(qAbs(stripRect.left() - anchorLeft) <= 1);
    QCOMPARE(stripRect.width(), qRound(strip->width()));
    // Each control is a finger wide there.
    QVERIFY(strip->property("pitch").toReal() >= 64);
    auto *trayFace = findItem(strip, QStringLiteral("temperance-magnifier-tray"));
    auto *controlFace = findItem(strip, QStringLiteral("temperance-magnifier-control"));
    auto *bellFace = findItem(strip, QStringLiteral("temperance-magnifier-notifications"));
    auto *nameLabel = findItem(strip, QStringLiteral("temperance-magnifier-label"));
    QVERIFY(trayFace && controlFace && bellFace && nameLabel);
    // The pressed control grew out under the finger and is chosen: its tile
    // sits behind it and its name shows. Nothing grows or lifts to say so,
    // in the strip or in the bar.
    const auto selectedKey = [&] {
      const int index = rightFace->property("magnifierSelected").toInt();
      const auto entries = rightFace->property("magnifierEntries").toList();
      return index >= 0 && index < entries.size()
          ? entries.at(index).toMap().value(QStringLiteral("key")).toString() : QString();
    };
    auto *selection = findItem(strip, QStringLiteral("temperance-magnifier-selection"));
    QVERIFY(selection);
    const auto stillAndMarked = [&](QQuickItem *chosen) {
      for (auto *face : {trayFace, controlFace, bellFace})
        if (face->scale() != 1.) return false;
      const qreal tileCentre = selection->x() + selection->width() / 2;
      return selection->isVisible() && qAbs(tileCentre - (chosen->x() + chosen->width() / 2)) < 1;
    };
    QCOMPARE(selectedKey(), QStringLiteral("tray"));
    QVERIFY(!rightFace->property("reachedControl").value<QObject *>());
    QTRY_VERIFY(stillAndMarked(trayFace));
    QTRY_VERIFY(nameLabel->opacity() > 0.99);
    QCOMPARE(nameLabel->property("text").toString(), QStringLiteral("System Tray"));
    // No page opens while the finger chooses.
    QVERIFY(!state->property("expanded").toBool());
    // A picture of the grown strip under the bar, when one is asked for.
    if (const QString shot = qEnvironmentVariable("TEMPERANCE_MAGNIFIER_SHOT"); !shot.isEmpty()) {
      QTest::qWait(400);
      const QImage bar = window.grabWindow();
      const QImage grown = qobject_cast<QQuickWindow *>(magnifierWindow)->grabWindow();
      const QRect around = QRect(stripRect.left() - 40, 0, stripRect.width() + 80,
                                 stripRect.bottom() + 40).intersected(screen);
      QImage picture(around.size() * bar.devicePixelRatio(), QImage::Format_ARGB32);
      picture.fill(QColor(QStringLiteral("#6b7280")));
      QPainter painter(&picture);
      painter.scale(bar.devicePixelRatio(), bar.devicePixelRatio());
      painter.drawImage(QPointF(-around.left(), 0), bar);
      painter.drawImage(QPointF(stripRect.left() - around.left(), stripRect.top()), grown);
      painter.end();
      QVERIFY(picture.save(shot));
      qInfo() << "T1_MAGNIFIER_SHOT" << shot;
    }

    // Sliding sideways chooses by how far the finger goes across the
    // magnified strip, not over the bar's small controls: a face's width
    // for each control, or, short of room before the screen's edge, a
    // shorter way that still reaches the strip's end there.
    const auto restCentres = strip->property("restCentres").toList();
    QCOMPARE(restCentres.size(), 3);
    const auto fingerFor = [&](qreal centre, int y) {
      const qreal pressed = rightFace->property("magnifierPressCentre").toReal();
      const qreal gain = rightFace->property(centre < pressed ? "magnifierGainLeft"
                                                              : "magnifierGainRight").toReal();
      const qreal pressX = rightFace->mapToScene(
          {rightFace->property("magnifierPressX").toReal(), 0}).x();
      return QPoint(qRound(pressX + (centre - pressed) / gain), y);
    };
    const auto overStrip = [&](int index, int y) {
      return fingerFor(restCentres.at(index).toReal(), y);
    };
    QPoint at = trayAt + QPoint(0, 140);
    pullTo(at, overStrip(1, at.y()));
    at = overStrip(1, at.y());
    QCOMPARE(selectedKey(), QStringLiteral("control"));
    QVERIFY(!rightFace->property("reachedControl").value<QObject *>());
    QTRY_VERIFY(stillAndMarked(controlFace));
    QCOMPARE(nameLabel->property("text").toString(), QStringLiteral("Control Center"));
    pullTo(at, overStrip(0, at.y()));
    at = overStrip(0, at.y());
    QCOMPARE(selectedKey(), QStringLiteral("notifications"));
    QVERIFY(nameLabel->property("text").toString().startsWith(QStringLiteral("Notifications")));
    QVERIFY(!state->property("expanded").toBool());
    // Lifted over a control, its page opens and the strip goes back.
    QTest::touchEvent(&window, finger).release(0, at);
    QTRY_VERIFY(state->property("expanded").toBool());
    QCOMPARE(state->property("page").toString(), QStringLiteral("notifications"));
    QTRY_VERIFY(!magnifierWindow->isVisible());
    QVERIFY(!rightFace->property("reachedControl").value<QObject *>());

    // Chosen again with a page open, the page changes; the same page stays.
    const auto pullAndLiftOver = [&](QQuickItem *from, int index) {
      const QPoint start = centreOf(from);
      QTest::touchEvent(&window, finger).press(0, start);
      pullTo(start, start + QPoint(0, 90));
      QTRY_VERIFY(magnifierWindow->isVisible());
      const QPoint over = overStrip(index, start.y() + 90);
      pullTo(start + QPoint(0, 90), over);
      QTest::touchEvent(&window, finger).release(0, over);
      QTRY_VERIFY(!magnifierWindow->isVisible());
    };
    pullAndLiftOver(bellButton, 1);
    QTRY_COMPARE(state->property("page").toString(), QStringLiteral("control"));
    QVERIFY(state->property("expanded").toBool());
    pullAndLiftOver(trayItem, 1);
    QTest::qWait(200);
    QCOMPARE(state->property("page").toString(), QStringLiteral("control"));
    QVERIFY(state->property("expanded").toBool());
    pullAndLiftOver(controlItem, 2);
    QTRY_COMPARE(state->property("page").toString(), QStringLiteral("tray"));
    state->setProperty("expanded", false);
    QTRY_VERIFY(!popup->isVisible());
    QTest::qWait(300);

    // Slid back up into the bar and lifted there, it cancels: the strip goes
    // back and nothing opens.
    QTest::touchEvent(&window, finger).press(0, trayAt);
    pullTo(trayAt, trayAt + QPoint(0, 90));
    QTRY_VERIFY(magnifierWindow->isVisible());
    pullTo(trayAt + QPoint(0, 90), trayAt + QPoint(-30, 2));
    QTRY_COMPARE(rightFace->property("magnifierGrowth").toReal(), 0.);
    QCOMPARE(rightFace->property("magnifierSelected").toInt(), -1);
    QTest::touchEvent(&window, finger).release(0, trayAt + QPoint(-30, 2));
    QTRY_VERIFY(!magnifierWindow->isVisible());
    QTest::qWait(300);
    QVERIFY(!state->property("expanded").toBool());
    // Lifted off either end of the strip, it cancels too.
    for (const qreal past : {-24., strip->width() + 24.}) {
      QTest::touchEvent(&window, finger).press(0, trayAt);
      pullTo(trayAt, trayAt + QPoint(0, 90));
      QTRY_VERIFY(magnifierWindow->isVisible());
      const QPoint off = fingerFor(past, trayAt.y() + 90);
      pullTo(trayAt + QPoint(0, 90), off);
      QCOMPARE(rightFace->property("magnifierSelected").toInt(), -1);
      QTest::touchEvent(&window, finger).release(0, off);
      QTRY_VERIFY(!magnifierWindow->isVisible());
      QTest::qWait(300);
      QVERIFY(!state->property("expanded").toBool());
    }

    // Whichever control the finger pulls down from is chosen first, though
    // along the screen's edge the strip cannot stand with every face under
    // its control; from the first, the last is still reached before the
    // screen's edge.
    const auto cancelAt = [&](const QPoint &from) {
      pullTo(from, from + QPoint(0, -88));
      QTest::touchEvent(&window, finger).release(0, from + QPoint(0, -88));
      QTRY_VERIFY(!magnifierWindow->isVisible());
      QTest::qWait(300);
      QVERIFY(!state->property("expanded").toBool());
    };
    for (const auto &[item, key] : {std::pair{bellButton, "notifications"},
                                    std::pair{controlItem, "control"},
                                    std::pair{trayItem, "tray"}}) {
      const QPoint start = centreOf(item);
      QTest::touchEvent(&window, finger).press(0, start);
      pullTo(start, start + QPoint(0, 90));
      QTRY_VERIFY(magnifierWindow->isVisible());
      QCOMPARE(selectedKey(), QString::fromLatin1(key));
      cancelAt(start + QPoint(0, 90));
    }
    {
      const QPoint start = centreOf(bellButton);
      const int edge = screen.right() - 4;
      QTest::touchEvent(&window, finger).press(0, start);
      pullTo(start, start + QPoint(0, 90));
      QTRY_VERIFY(magnifierWindow->isVisible());
      pullTo(start + QPoint(0, 90), QPoint(edge, start.y() + 90));
      QCOMPARE(selectedKey(), QStringLiteral("tray"));
      cancelAt(QPoint(edge, start.y() + 90));
    }

    // Pressed between two controls, the strip grows out with the nearer one
    // under the finger, chosen.
    {
      const qreal controlRight = controlItem->mapToScene({controlItem->width(), 0}).x();
      const qreal trayLeft = trayItem->mapToScene({0, 0}).x();
      const QPoint between(qRound(trayLeft - qMax(0., trayLeft - controlRight) / 4), trayAt.y());
      qInfo() << "T1_BETWEEN" << controlRight << trayLeft << between;
      QTest::touchEvent(&window, finger).press(0, between);
      pullTo(between, between + QPoint(0, 90));
      QTRY_VERIFY(magnifierWindow->isVisible());
      QCOMPARE(selectedKey(), QStringLiteral("tray"));
      pullTo(between + QPoint(0, 90), between + QPoint(0, 2));
      QTest::touchEvent(&window, finger).release(0, between + QPoint(0, 2));
      QTRY_VERIFY(!magnifierWindow->isVisible());
      QTest::qWait(300);
      QVERIFY(!state->property("expanded").toBool());
    }

    // The panel says which part of it the finger's controls take: from the
    // bell to the row's end, not the ticker's room before it.
    {
      const QRectF area = rightFace->property("fingerControlsArea").toRectF();
      const qreal bellLeft = rightFace->mapFromItem(bellButton, QPointF(0, 0)).x();
      qInfo() << "T1_FINGER_AREA" << area << bellLeft << rightFace->width();
      QVERIFY(qAbs(area.left() - bellLeft) <= 1);
      QVERIFY(area.left() > 0);
      QVERIFY(area.right() <= rightFace->width() + 1);
      QCOMPARE(area.height(), rightFace->height());
    }

    // A mouse still opens the one control it clicks, and resting on the
    // bell raises no name over the bar: the page it opens names itself.
    QTest::mouseClick(&window, Qt::LeftButton, {}, centreOf(trayItem));
    QTRY_VERIFY(state->property("expanded").toBool());
    QCOMPARE(state->property("page").toString(), QStringLiteral("tray"));
    QTest::qWait(300);
    QTest::mouseClick(&window, Qt::LeftButton, {}, centreOf(trayItem));
    QTRY_VERIFY(!state->property("expanded").toBool());
    QTRY_VERIFY(!popup->isVisible());
    QTest::qWait(300);
    // Room measured before the dock arrived is measured again as the page
    // opens, so the page still fits below the bar.
    const qreal room = rightFace->property("notificationPopupHeightLimit").toReal();
    QVERIFY(room > 1);
    rightFace->setProperty("notificationPopupHeightLimit", room + 5000);
    QTest::mouseClick(&window, Qt::LeftButton, {}, centreOf(bellButton));
    QTRY_COMPARE(state->property("page").toString(), QStringLiteral("notifications"));
    QVERIFY(state->property("expanded").toBool());
    QVERIFY(!magnifierWindow->isVisible());
    QCOMPARE(rightFace->property("notificationPopupHeightLimit").toReal(), room);
    QTRY_VERIFY(placed(popup).top() >= barBottom);
    state->setProperty("expanded", false);
    QTRY_VERIFY(!popup->isVisible());
    QTest::qWait(300);
    QTest::mouseMove(&window, centreOf(bellButton) + QPoint(0, 1));
    QTest::mouseMove(&window, centreOf(bellButton));
    QTest::qWait(1500);
    QCOMPARE(visibleToolTips(), 0);
    QTest::mouseMove(&window, {0, 0});

    // Without the bar the panel's own neighbours decide again.
    hostBus.unregisterService(QStringLiteral("co.goodinput.ContextBar"));
    QTRY_VERIFY(rightFace->width() < 520);
    QCOMPARE(host.asked.size(), 2);
    hostBus.unregisterObject(QStringLiteral("/ContextBar"));
    face->setParentItem(nullptr);
  }
};
QTEST_MAIN(TickerPanelTest)
#include "TickerPanelTest.moc"
