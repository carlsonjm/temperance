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
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFileInfo>
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
    // A mouse over a header control lightens it, in tablet mode too, where
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
      QTRY_VERIFY(face->property("color").value<QColor>().lightness() > resting.lightness());
      qInfo() << "T1_HOVER" << name << resting.name()
              << face->property("color").value<QColor>().name();
      QTest::mouseMove(popupWindow, {2, 2});
    };
    hoverLightens(QStringLiteral("temperance-header-lock"));
    hoverLightens(QStringLiteral("temperance-session-more"));
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
    face->setParentItem(nullptr);
    qInfo() << "T1_ERRORS" << boundaryError << surfaceError;
    QVERIFY(boundaryError <= 2.);
    // The root's actual allocation, not its preferred hint, owns the rail.
    // Native layout gaps can leave a few pixels below the requested width.
  }
};
QTEST_MAIN(TickerPanelTest)
#include "TickerPanelTest.moc"
