// SPDX-License-Identifier: LGPL-2.0-or-later
// Today's events as the ticker and the history take them (TodayEvents.qml):
// timed events that have not ended, an overnight one past midnight, and the
// seen and dismissed marks, which hold across midnight for an event still
// under way.
#include <KLocalizedContext>

#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QJSValue>
#include <QQmlPropertyMap>

#include <memory>

static int failures = 0;

static void check(bool condition, const QString &message)
{
    if (condition) return;
    ++failures;
    qWarning().noquote() << "FAIL:" << message;
}

// What QML hands back, a script value or not, as plain lists and maps.
static QVariant plain(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QJSValue>())
        return value.value<QJSValue>().toVariant();
    return value;
}

static QStringList strings(const QVariant &value)
{
    return plain(value).toStringList();
}

static QStringList titles(const QVariant &list)
{
    QStringList result;
    for (const QVariant &event : plain(list).toList())
        result.append(event.toMap().value(QStringLiteral("title")).toString());
    return result;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QQmlEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));

    // Half past midnight on the 11th: a call from 23:00 on the 10th runs to
    // 01:00, a breakfast is ahead, a dentist visit is over, and an all-day
    // birthday stays in the calendar.
    const QJSValue feeds = engine.evaluate(QStringLiteral(R"JS(({
        revision: 1,
        eventsForDay: function(year, month, day) {
            if (year !== 2026 || month !== 9 || day !== 11) return [];
            return [
                { title: "Birthday", allDay: true, key: "birthday" },
                { title: "Late call", allDay: true, continuing: true, key: "call",
                  start: new Date(2026, 8, 10, 23, 0), end: new Date(2026, 8, 11, 1, 0) },
                { title: "Dentist", allDay: false, key: "dentist",
                  start: new Date(2026, 8, 11, 0, 0), end: new Date(2026, 8, 11, 0, 15) },
                { title: "Breakfast", allDay: false, key: "breakfast",
                  start: new Date(2026, 8, 11, 7, 30), end: new Date(2026, 8, 11, 8, 0) }
            ];
        }
    }))JS"));
    const QJSValue clock = engine.evaluate(QStringLiteral(
        "({ amLabel: 'a.m.', pmLabel: 'p.m.', timeString: function(value) { return value.getHours() + ':' "
        "+ String(value.getMinutes()).padStart(2, '0') + ' a.m.'; } })"));
    QQmlPropertyMap configuration;
    configuration.insert(QStringLiteral("readEvents"), QStringList{});
    configuration.insert(QStringLiteral("dismissedEvents"), QStringList{});

    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(TODAY_EVENTS_QML)));
    std::unique_ptr<QObject> events(component.createWithInitialProperties({
        {QStringLiteral("feeds"), QVariant::fromValue(feeds)},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("now"), QDateTime(QDate(2026, 9, 11), QTime(0, 30))},
        {QStringLiteral("configuration"), QVariant::fromValue(&configuration)},
        {QStringLiteral("clock"), QVariant::fromValue(clock)},
    }));
    if (!events) {
        qCritical().noquote() << component.errorString();
        return 1;
    }

    check(titles(events->property("today")) == QStringList{QStringLiteral("Late call"), QStringLiteral("Breakfast")},
          QStringLiteral("today holds the overnight call and breakfast: %1")
              .arg(titles(events->property("today")).join(QStringLiteral(", "))));
    const QVariantMap next = plain(events->property("next")).toMap();
    check(next.value(QStringLiteral("title")) == QStringLiteral("Late call"), QStringLiteral("the call under way rides the ticker"));
    QVariant lead;
    QMetaObject::invokeMethod(events.get(), "timeText", Q_RETURN_ARG(QVariant, lead), Q_ARG(QVariant, next));
    check(lead.toString() == QStringLiteral("Now"), QStringLiteral("an event under way leads with Now, got %1").arg(lead.toString()));

    // A mark made before midnight still holds after it.
    configuration.insert(QStringLiteral("readEvents"), QStringList{QStringLiteral("2026-09-10|call")});
    check(plain(events->property("next")).toMap().value(QStringLiteral("title")) == QStringLiteral("Breakfast"),
          QStringLiteral("the call seen before midnight stays off the ticker"));
    configuration.insert(QStringLiteral("dismissedEvents"), QStringList{QStringLiteral("2026-09-10|call")});
    check(titles(events->property("today")) == QStringList{QStringLiteral("Breakfast")},
          QStringLiteral("the call dismissed before midnight stays dismissed"));

    // A new mark keeps yesterday's and today's, and drops older days'.
    configuration.insert(QStringLiteral("readEvents"),
                         QStringList{QStringLiteral("2026-09-09|old"), QStringLiteral("2026-09-10|call")});
    QMetaObject::invokeMethod(events.get(), "markSeen", Q_ARG(QVariant, QStringLiteral("breakfast")));
    check(strings(configuration.value(QStringLiteral("readEvents")))
              == QStringList{QStringLiteral("2026-09-10|call"), QStringLiteral("2026-09-11|breakfast")},
          QStringLiteral("marks keep yesterday's and today's: %1")
              .arg(strings(configuration.value(QStringLiteral("readEvents"))).join(QStringLiteral(", "))));

    // After the call ends, it is gone.
    events->setProperty("now", QDateTime(QDate(2026, 9, 11), QTime(1, 5)));
    configuration.insert(QStringLiteral("dismissedEvents"), QStringList{});
    check(titles(events->property("today")) == QStringList{QStringLiteral("Breakfast")},
          QStringLiteral("the call goes once it ends"));

    if (failures == 0) qInfo().noquote() << "PASS: today's events";
    return failures == 0 ? 0 : 1;
}
