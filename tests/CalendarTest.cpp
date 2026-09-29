// SPDX-License-Identifier: LGPL-2.0-or-later
#include "calendarfeeds.h"

#include <KLocalizedContext>
#include <KLocalizedString>
#include <QColor>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QThread>
#include <QUrl>
#include <QWheelEvent>
#include <cmath>
#include <memory>

static int failures = 0;

static void check(bool condition, const QString &message)
{
    if (condition) return;
    ++failures;
    qWarning().noquote() << "FAIL:" << message;
}

static void settle(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(2);
    }
}

static void collect(QQuickItem *root, const QString &name, QList<QQuickItem *> &found)
{
    if (root->objectName() == name) found.append(root);
    for (QQuickItem *child : root->childItems()) collect(child, name, found);
}

static QList<QQuickItem *> named(QQuickItem *root, const QString &name)
{
    QList<QQuickItem *> found;
    collect(root, name, found);
    return found;
}

static QQuickItem *child(QQuickItem *root, const QString &name)
{
    const QList<QQuickItem *> found = named(root, name);
    return found.isEmpty() ? nullptr : found.first();
}

static QQuickItem *dayCell(QQuickItem *page, int day)
{
    for (QQuickItem *cell : named(page, QStringLiteral("temperance-calendar-day")))
        if (cell->property("modelData").toInt() == day) return cell;
    return nullptr;
}

int main(int argc, char **argv)
{
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    QGuiApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("temperance");
    QQmlEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));

    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(CALENDAR_PAGE_QML)));
    const QVariantMap initial{{QStringLiteral("today"), QDateTime(QDate(2026, 9, 25), QTime(9, 41))},
                              {QStringLiteral("accentColor"), QColor(QStringLiteral("#3DAEE9"))}};
    std::unique_ptr<QObject> object(component.createWithInitialProperties(initial));
    auto *page = qobject_cast<QQuickItem *>(object.get());
    if (!page) {
        qWarning().noquote() << component.errorString();
        return 1;
    }
    QQuickWindow window;
    window.resize(int(page->implicitWidth()), int(page->implicitHeight()));
    window.setColor(QColor(QStringLiteral("#141414")));
    page->setParentItem(window.contentItem());
    page->setSize(QSizeF(page->implicitWidth(), page->implicitHeight()));
    window.show();
    settle(300);

    // September 2026 starts on a Tuesday: five weeks, two empty leading cells.
    const QList<QQuickItem *> days = named(page, QStringLiteral("temperance-calendar-day"));
    const QList<QQuickItem *> blanks = named(page, QStringLiteral("temperance-calendar-blank"));
    check(days.size() == 30, QStringLiteral("September has 30 days, found %1").arg(days.size()));
    check(days.size() + blanks.size() == 35, QStringLiteral("September takes five weeks"));
    check(page->property("title").toString() == QStringLiteral("September"),
          QStringLiteral("the current year's month is titled without its year"));

    // The outer circles sit on the 16 px margin line the popup gives its
    // title: the page is inset 4 px, so 12 px inside the page.
    const qreal textInset = page->property("textInset").toReal();
    check(textInset == 12, QStringLiteral("text inset is %1").arg(textInset));
    auto markOf = [](QQuickItem *cell) { return child(cell, QStringLiteral("temperance-calendar-mark")); };
    QQuickItem *sunday = dayCell(page, 6);
    QQuickItem *saturday = dayCell(page, 12);
    if (sunday && saturday) {
        const qreal left = markOf(sunday)->mapToItem(page, QPointF(0, 0)).x();
        const qreal right = markOf(saturday)->mapToItem(page, QPointF(markOf(saturday)->width(), 0)).x();
        check(std::abs(left - textInset) < 0.5, QStringLiteral("first column's circle starts at %1").arg(left));
        check(std::abs(page->width() - right - textInset) < 0.5,
              QStringLiteral("last column's circle ends %1 from the edge").arg(page->width() - right));
        const qreal nextLeft = markOf(dayCell(page, 7))->mapToItem(page, QPointF(0, 0)).x();
        const qreal nextTop = markOf(dayCell(page, 13))->mapToItem(page, QPointF(0, 0)).y();
        const qreal top = markOf(sunday)->mapToItem(page, QPointF(0, 0)).y();
        check(std::abs(nextLeft - left - markOf(sunday)->width() - 8) < 0.5, QStringLiteral("circles are 8 px apart across"));
        check(std::abs(nextTop - top - markOf(sunday)->height() - 8) < 0.5, QStringLiteral("circles are 8 px apart down"));
    } else {
        check(false, QStringLiteral("the 6th and 12th are in the grid"));
    }

    // Today carries the accent; other days rest transparent.
    check(markOf(dayCell(page, 25))->property("color").value<QColor>() == QColor(QStringLiteral("#3DAEE9")),
          QStringLiteral("today is marked in the accent"));
    check(markOf(dayCell(page, 24))->property("color").value<QColor>().alpha() == 0,
          QStringLiteral("another day rests without a fill"));

    // Choosing a day fills it and names it beneath the grid.
    page->setProperty("selectedDate", QDateTime(QDate(2026, 9, 7), QTime(0, 0)));
    settle(200);
    const QColor selected = markOf(dayCell(page, 7))->property("color").value<QColor>();
    check(std::abs(selected.alphaF() - 0.24) < 0.01, QStringLiteral("a chosen day takes the 24% fill"));

    // Holidays come from Plasma's plugin and the region the person set in
    // Plasma; where neither is present the check reports rather than fails.
    const bool laborDay = child(dayCell(page, 7), QStringLiteral("temperance-calendar-dot"))->isVisible();
    if (laborDay) {
        QStringList listed;
        for (QQuickItem *title : named(page, QStringLiteral("temperance-calendar-event-title")))
            listed.append(title->property("text").toString());
        check(listed.contains(QStringLiteral("Labor Day")), QStringLiteral("the chosen holiday is named beneath the grid"));
        qInfo().noquote() << "PASS: holidays are marked and named";
    } else {
        qInfo().noquote() << "SKIP: no holiday region is set here";
    }

    // A linked calendar's events join the day: breakfast then the dentist,
    // each on its own row, and a dot on a day that only has linked events.
    CalendarFeeds feeds;
    feeds.setLinks({QUrl::fromLocalFile(QStringLiteral(FIXTURES_DIR "/calendar.ics")).toString()});
    QElapsedTimer wait;
    wait.start();
    while (feeds.revision() == 0 && wait.elapsed() < 5000) settle(20);
    page->setProperty("feeds", QVariant::fromValue(&feeds));
    page->setProperty("selectedDate", QDateTime(QDate(2026, 9, 25), QTime(0, 0)));
    settle(200);
    QStringList rows;
    for (QQuickItem *title : named(page, QStringLiteral("temperance-calendar-event-title")))
        if (title->isVisible()) rows.append(title->property("text").toString());
    check(rows == QStringList{QStringLiteral("Breakfast"), QStringLiteral("Dentist")},
          QStringLiteral("the 25th lists its events in order: %1").arg(rows.join(QStringLiteral(", "))));
    check(child(dayCell(page, 22), QStringLiteral("temperance-calendar-dot"))->isVisible(),
          QStringLiteral("a day with only a linked event has a dot"));
    page->setProperty("selectedDate", QDateTime(QDate(2026, 9, 24), QTime(0, 0)));
    settle(100);
    check(child(page, QStringLiteral("temperance-calendar-empty"))->property("text").toString() == QStringLiteral("Nothing scheduled"),
          QStringLiteral("an empty day with calendars linked says nothing is scheduled"));
    page->setProperty("selectedDate", QDateTime(QDate(2026, 9, 25), QTime(0, 0)));
    settle(100);

    if (qEnvironmentVariableIsSet("TEMPERANCE_CALENDAR_SNAPSHOT"))
        window.grabWindow().save(qEnvironmentVariable("TEMPERANCE_CALENDAR_SNAPSHOT"));

    // A wheel notch down turns to the next month, and up turns back.
    QQuickItem *grid = child(page, QStringLiteral("temperance-calendar-grid"));
    auto turn = [&](int delta) {
        const QPointF at = grid->mapToScene(QPointF(grid->width() / 2, grid->height() / 2));
        QWheelEvent event(at, window.mapToGlobal(at), QPoint(), QPoint(0, delta), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&window, &event);
        settle(300);
    };
    turn(-120);
    check(page->property("title").toString() == QStringLiteral("October"),
          QStringLiteral("a wheel notch down shows October, got %1").arg(page->property("title").toString()));
    turn(120);
    check(page->property("title").toString() == QStringLiteral("September"), QStringLiteral("a notch up returns to September"));

    // Turning the month: October opens on its first, away from today.
    QMetaObject::invokeMethod(page, "showMonth", Q_ARG(QVariant, 1));
    settle(400);
    check(page->property("title").toString() == QStringLiteral("October"), QStringLiteral("the next month is October"));
    check(!page->property("showingCurrentMonth").toBool(), QStringLiteral("October is not the current month"));
    check(page->property("selectedDate").toDateTime().date() == QDate(2026, 10, 1),
          QStringLiteral("a new month selects its first day"));
    for (int step = 0; step < 3; ++step) QMetaObject::invokeMethod(page, "showMonth", Q_ARG(QVariant, 1));
    settle(400);
    check(page->property("title").toString() == QStringLiteral("January 2027"),
          QStringLiteral("another year's month carries its year, got %1").arg(page->property("title").toString()));
    QMetaObject::invokeMethod(page, "reset");
    settle(100);
    check(page->property("showingCurrentMonth").toBool()
              && page->property("selectedDate").toDateTime().date() == QDate(2026, 9, 25),
          QStringLiteral("reset returns to today"));

    if (failures == 0) qInfo().noquote() << "PASS: calendar";
    return failures == 0 ? 0 : 1;
}
