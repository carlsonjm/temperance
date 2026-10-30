// SPDX-License-Identifier: LGPL-2.0-or-later
#include "calendarfeeds.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>

#include <functional>

static int failures = 0;

static void check(bool condition, const QString &message)
{
    if (condition) return;
    ++failures;
    qWarning().noquote() << "FAIL:" << message;
}

static bool waitFor(const std::function<bool()> &condition, int milliseconds = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(2);
    }
    return condition();
}

static QString fixture(const QString &name)
{
    return QUrl::fromLocalFile(QStringLiteral(FIXTURES_DIR "/") + name).toString();
}

static QStringList titles(const QVariantMap &month, int day)
{
    QStringList result;
    for (const QVariant &event : month.value(QString::number(day)).toList())
        result.append(event.toMap().value(QStringLiteral("title")).toString());
    return result;
}

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(data);
}

static QByteArray invitation(const QByteArray &uid, const QByteArray &summary, const QByteArray &status)
{
    return "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//test//EN\r\nBEGIN:VEVENT\r\nUID:" + uid
        + "\r\nDTSTAMP:20261001T000000Z\r\nDTSTART:20261103T150000Z\r\nDTEND:20261103T160000Z\r\nSUMMARY:"
        + summary + "\r\n" + (status.isEmpty() ? QByteArray() : "STATUS:" + status + "\r\n")
        + "END:VEVENT\r\nEND:VCALENDAR\r\n";
}

static QVariantMap feedAt(CalendarFeeds &feeds, int index)
{
    const QVariantList list = feeds.feeds();
    return index < list.size() ? list.at(index).toMap() : QVariantMap();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // A webcal address is the same feed over https; text that is not a web
    // address is refused.
    check(CalendarFeeds::normalizedLink(QStringLiteral("  webcal://p01.icloud.com/published/2/abc  "))
              == QStringLiteral("https://p01.icloud.com/published/2/abc"),
          QStringLiteral("webcal becomes https"));
    // A private address is a secret: one pasted as plain http is read over
    // https, never in the clear.
    check(CalendarFeeds::normalizedLink(QStringLiteral("http://calendar.example.com/private/abc.ics"))
              == QStringLiteral("https://calendar.example.com/private/abc.ics"),
          QStringLiteral("http becomes https"));
    check(CalendarFeeds::normalizedLink(QStringLiteral("HTTP://calendar.example.com/a.ics"))
              == QStringLiteral("https://calendar.example.com/a.ics"),
          QStringLiteral("an upper-case HTTP becomes https"));
    check(CalendarFeeds::normalizedLink(QStringLiteral("my calendar")).isEmpty(), QStringLiteral("plain text is refused"));
    check(CalendarFeeds::normalizedLink(QStringLiteral("ftp://example.com/a.ics")).isEmpty(),
          QStringLiteral("other schemes are refused"));

    CalendarFeeds feeds;
    feeds.setLinks({fixture(QStringLiteral("calendar.ics")), fixture(QStringLiteral("calendar.ics")),
                    QStringLiteral("not a link")});
    check(feeds.links().size() == 1, QStringLiteral("duplicates and non-links are dropped"));
    check(feedAt(feeds, 0).value(QStringLiteral("state")) == QStringLiteral("loading"),
          QStringLiteral("a new link reads as loading"));
    check(waitFor([&] { return feedAt(feeds, 0).value(QStringLiteral("state")) == QStringLiteral("ready"); }),
          QStringLiteral("the calendar is read"));
    check(feedAt(feeds, 0).value(QStringLiteral("name")) == QStringLiteral("Home"), QStringLiteral("the feed's own name is used"));

    const QVariantMap september = feeds.eventsForMonth(2026, 9);
    // Timed events in order of start; the cancelled lunch is not there.
    check(titles(september, 25) == QStringList{QStringLiteral("Breakfast"), QStringLiteral("Dentist")},
          QStringLiteral("the 25th lists breakfast then the dentist: %1").arg(titles(september, 25).join(QStringLiteral(", "))));
    const QVariantMap dentist = september.value(QStringLiteral("25")).toList().value(1).toMap();
    check(!dentist.value(QStringLiteral("allDay")).toBool()
              && dentist.value(QStringLiteral("start")).toDateTime().time() == QTime(10, 0)
              && dentist.value(QStringLiteral("end")).toDateTime().time() == QTime(11, 0),
          QStringLiteral("the dentist runs from 10:00 to 11:00"));
    check(dentist.value(QStringLiteral("key")).toString().startsWith(QStringLiteral("dentist@test|")),
          QStringLiteral("an occurrence carries a key naming it"));
    const QVariantMap first = feeds.eventsForMonth(2026, 9).value(QStringLiteral("25")).toList().value(1).toMap();
    check(first.value(QStringLiteral("key")) == dentist.value(QStringLiteral("key")),
          QStringLiteral("the key is the same when read again"));
    const QString standup7 = september.value(QStringLiteral("7")).toList().value(0).toMap().value(QStringLiteral("key")).toString();
    const QString standup28 = september.value(QStringLiteral("28")).toList().value(1).toMap().value(QStringLiteral("key")).toString();
    check(!standup7.isEmpty() && standup7 != standup28, QStringLiteral("each occurrence of a repeat has its own key"));
    check(titles(september, 26).isEmpty(), QStringLiteral("a cancelled event is left out"));
    check(titles(september, 28) == QStringList{QStringLiteral("Birthday"), QStringLiteral("Standup")},
          QStringLiteral("an all-day event comes before the day's timed ones: %1").arg(titles(september, 28).join(QStringLiteral(", "))));
    check(titles(september, 29) == QStringList{QStringLiteral("Trip")} && titles(september, 30) == QStringList{QStringLiteral("Trip")},
          QStringLiteral("a three-day event covers each of its days"));
    // The weekly standup: on the 7th and 28th, gone on the 14th, and moved
    // from the 21st to the 22nd.
    check(titles(september, 7) == QStringList{QStringLiteral("Standup")}, QStringLiteral("a repeat's first day"));
    check(titles(september, 14).isEmpty(), QStringLiteral("a deleted repeat is gone"));
    check(titles(september, 21).isEmpty(), QStringLiteral("a moved repeat leaves its day"));
    check(titles(september, 22) == QStringList{QStringLiteral("Standup (moved)")}, QStringLiteral("a moved repeat is on its new day"));

    // An overnight event starts on its first day and is under way on the
    // next, where it keeps its real times so the ticker can follow it.
    const QVariantMap lateStart = september.value(QStringLiteral("10")).toList().value(0).toMap();
    const QVariantMap lateRest = september.value(QStringLiteral("11")).toList().value(0).toMap();
    check(titles(september, 10) == QStringList{QStringLiteral("Late call")}
              && !lateStart.value(QStringLiteral("allDay")).toBool()
              && !lateStart.value(QStringLiteral("continuing")).toBool(),
          QStringLiteral("an overnight event starts on its first day"));
    check(titles(september, 11) == QStringList{QStringLiteral("Late call")}
              && lateRest.value(QStringLiteral("allDay")).toBool()
              && lateRest.value(QStringLiteral("continuing")).toBool()
              && lateRest.value(QStringLiteral("start")).toDateTime().time() == QTime(23, 0)
              && lateRest.value(QStringLiteral("end")).toDateTime() == QDateTime(QDate(2026, 9, 11), QTime(1, 0)),
          QStringLiteral("after midnight it is under way, with its real start and end"));
    check(lateRest.value(QStringLiteral("key")) == lateStart.value(QStringLiteral("key")),
          QStringLiteral("both days name the same occurrence"));

    // A day is the same as its place in the month, and reading the month
    // again gives the same events.
    check(feeds.eventsForDay(2026, 9, 25) == september.value(QStringLiteral("25")).toList(),
          QStringLiteral("a day's events are its place in the month"));
    check(feeds.eventsForMonth(2026, 9) == september, QStringLiteral("a month read again is the same"));

    const QVariantMap october = feeds.eventsForMonth(2026, 10);
    check(titles(october, 1) == QStringList{QStringLiteral("Trip")}, QStringLiteral("the trip's last day is in October"));
    check(titles(october, 5) == QStringList{QStringLiteral("Standup")}, QStringLiteral("the repeat continues into October"));

    // An event's own colour wins, then its calendar's; Apple's #RRGGBBAA is
    // reordered for QML. A feed with neither leaves the colour empty.
    CalendarFeeds colored;
    colored.setLinks({fixture(QStringLiteral("colored.ics"))});
    check(waitFor([&] { return feedAt(colored, 0).value(QStringLiteral("state")) == QStringLiteral("ready"); }),
          QStringLiteral("the coloured calendar is read"));
    const QVariantList work = colored.eventsForMonth(2026, 9).value(QStringLiteral("25")).toList();
    check(work.value(0).toMap().value(QStringLiteral("color")) == QStringLiteral("#FFFF2968"),
          QStringLiteral("an event takes its calendar's colour: %1").arg(work.value(0).toMap().value(QStringLiteral("color")).toString()));
    check(work.value(1).toMap().value(QStringLiteral("color")) == QStringLiteral("turquoise"),
          QStringLiteral("an event's own colour wins"));
    check(dentist.value(QStringLiteral("color")).toString().isEmpty(), QStringLiteral("a feed without colours gives none"));
    // The person's pick for a calendar wins over the feed's, not over an
    // event's own.
    colored.setColors({{colored.links().value(0), QStringLiteral("#3DAEE9")}});
    const QVariantList picked = colored.eventsForMonth(2026, 9).value(QStringLiteral("25")).toList();
    check(picked.value(0).toMap().value(QStringLiteral("color")) == QStringLiteral("#3DAEE9"),
          QStringLiteral("a picked colour replaces the calendar's"));
    check(picked.value(1).toMap().value(QStringLiteral("color")) == QStringLiteral("turquoise"),
          QStringLiteral("an event's own colour still wins over a pick"));
    // A colour saved under a link's earlier spelling still finds it.
    CalendarFeeds respelled;
    respelled.setColors({{QStringLiteral("http://calendar.example.com/a.ics"), QStringLiteral("#3DAEE9")}});
    check(respelled.colors().value(QStringLiteral("https://calendar.example.com/a.ics")) == QStringLiteral("#3DAEE9"),
          QStringLiteral("a colour picked for an http link follows it to https"));

    // A link that fails reports it; a file that is not a calendar says so.
    CalendarFeeds broken;
    broken.setLinks({fixture(QStringLiteral("missing.ics")), fixture(QStringLiteral("not-a-calendar.txt"))});
    check(waitFor([&] {
              return feedAt(broken, 0).value(QStringLiteral("state")) == QStringLiteral("failed")
                  && feedAt(broken, 1).value(QStringLiteral("state")) == QStringLiteral("failed");
          }),
          QStringLiteral("both broken links fail"));
    check(!feedAt(broken, 1).value(QStringLiteral("message")).toString().isEmpty(), QStringLiteral("a failure carries a message"));

    // Removing a link removes its events.
    const int before = feeds.revision();
    feeds.setLinks({});
    check(feeds.revision() > before && feeds.eventsForMonth(2026, 9).isEmpty(), QStringLiteral("an unlinked calendar's events go"));

    // The local folder: one file per event, hidden files left alone, read
    // again when it changes, and made after it is first asked for.
    QTemporaryDir home;
    const QString folder = home.path() + QStringLiteral("/temperance/calendar");
    CalendarFeeds local;
    local.setLocalFolder(folder);
    check(local.eventsForMonth(2026, 11).isEmpty(), QStringLiteral("a missing local folder has no events"));
    QDir().mkpath(folder);
    writeFile(folder + QStringLiteral("/standup.ics"), invitation("standup", "Stand-up", "CONFIRMED"));
    writeFile(folder + QStringLiteral("/.half-written.ics.tmp"), invitation("hidden", "Hidden", ""));
    writeFile(folder + QStringLiteral("/.hidden.ics"), invitation("hidden2", "Hidden", ""));
    check(waitFor([&] { return titles(local.eventsForMonth(2026, 11), 3) == QStringList{QStringLiteral("Stand-up")}; }),
          QStringLiteral("an event written to the new local folder appears, and hidden files do not"));
    writeFile(folder + QStringLiteral("/review.ics"), invitation("review", "Review", "TENTATIVE"));
    writeFile(folder + QStringLiteral("/broken.ics"), "BEGIN:VCALENDAR\r\nnot really\r\n");
    check(waitFor([&] { return local.eventsForMonth(2026, 11).value(QStringLiteral("3")).toList().size() == 2; }),
          QStringLiteral("a second local event appears beside the first, and a broken file is skipped"));
    for (const QVariant &event : local.eventsForMonth(2026, 11).value(QStringLiteral("3")).toList()) {
        const QVariantMap map = event.toMap();
        check(map.value(QStringLiteral("tentative")).toBool() == (map.value(QStringLiteral("title")) == QStringLiteral("Review")),
              QStringLiteral("only the event answered maybe is tentative"));
    }
    QFile::remove(folder + QStringLiteral("/standup.ics"));
    check(waitFor([&] { return titles(local.eventsForMonth(2026, 11), 3) == QStringList{QStringLiteral("Review")}; }),
          QStringLiteral("a removed local event goes"));

    if (failures == 0) qInfo().noquote() << "PASS: calendar feeds";
    return failures == 0 ? 0 : 1;
}
