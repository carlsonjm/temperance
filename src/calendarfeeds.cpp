/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "calendarfeeds.h"

#include <KCalendarCore/ICalFormat>
#include <KLocalizedString>

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimeZone>
#include <QUrl>

#include <algorithm>

namespace
{
constexpr int defaultRefreshInterval = 10 * 60 * 1000;
constexpr int fetchTimeout = 30 * 1000;

// Most feeds name themselves; the address's host stands in for one that
// does not.
QString calendarName(const QByteArray &data, const QString &link)
{
    const int start = data.indexOf("X-WR-CALNAME:");
    if (start >= 0) {
        const int from = start + int(qstrlen("X-WR-CALNAME:"));
        int end = data.indexOf('\n', from);
        if (end < 0)
            end = data.size();
        const QString name = QString::fromUtf8(data.mid(from, end - from)).trimmed();
        if (!name.isEmpty())
            return name;
    }
    return QUrl(link).host();
}

// A feed's own colour: Apple's property, or the standard one on the calendar
// itself (RFC 7986). Apple writes #RRGGBBAA, which is turned into the #AARRGGBB
// order a QML colour reads.
QString calendarColor(const QByteArray &data)
{
    const int events = data.indexOf("BEGIN:VEVENT");
    const QByteArray head = events >= 0 ? data.left(events) : data;
    for (const QByteArray &property : {QByteArrayLiteral("\nX-APPLE-CALENDAR-COLOR:"), QByteArrayLiteral("\nCOLOR:")}) {
        const int start = head.indexOf(property);
        if (start < 0)
            continue;
        const int from = start + property.size();
        int end = head.indexOf('\n', from);
        if (end < 0)
            end = head.size();
        QString color = QString::fromUtf8(head.mid(from, end - from)).trimmed();
        if (color.size() == 9 && color.startsWith(QLatin1Char('#')))
            color = QLatin1Char('#') + color.mid(7, 2) + color.mid(1, 6);
        if (!color.isEmpty())
            return color;
    }
    return {};
}

QString failureMessage(QNetworkReply::NetworkError error)
{
    switch (error) {
    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::ContentNotFoundError:
    case QNetworkReply::ContentGoneError:
    case QNetworkReply::AuthenticationRequiredError:
        return i18nc("@info a linked calendar's status", "This link no longer opens the calendar");
    default:
        return i18nc("@info a linked calendar's status", "Couldn't reach this calendar");
    }
}
}

CalendarFeeds::CalendarFeeds(QObject *parent)
    : QObject(parent)
{
    m_refresh.setInterval(defaultRefreshInterval);
    connect(&m_refresh, &QTimer::timeout, this, &CalendarFeeds::refresh);
}

CalendarFeeds::~CalendarFeeds()
{
    for (Feed &feed : m_feeds) {
        if (feed.reply)
            feed.reply->abort();
    }
}

QStringList CalendarFeeds::links() const
{
    return m_links;
}

QVariantMap CalendarFeeds::colors() const
{
    return m_colors;
}

void CalendarFeeds::setColors(const QVariantMap &colors)
{
    if (colors == m_colors)
        return;
    m_colors = colors;
    ++m_revision;
    Q_EMIT colorsChanged();
    Q_EMIT eventsChanged();
}

QString CalendarFeeds::normalizedLink(const QString &link)
{
    QString text = link.trimmed();
    if (text.startsWith(QLatin1String("webcal://"), Qt::CaseInsensitive))
        text = QLatin1String("https://") + text.mid(int(qstrlen("webcal://")));
    const QUrl url(text, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid())
        return {};
    if ((scheme == QLatin1String("https") || scheme == QLatin1String("http")) && !url.host().isEmpty())
        return url.toString();
    if (scheme == QLatin1String("file") && !url.path().isEmpty())
        return url.toString();
    return {};
}

void CalendarFeeds::setLinks(const QStringList &links)
{
    QStringList normalized;
    for (const QString &link : links) {
        const QString value = normalizedLink(link);
        if (!value.isEmpty() && !normalized.contains(value))
            normalized.append(value);
    }
    if (normalized == m_links)
        return;

    // A link that stays keeps what it has already read.
    QList<Feed> next;
    bool lostEvents = false;
    for (const QString &link : std::as_const(normalized)) {
        const auto existing = std::find_if(m_feeds.begin(), m_feeds.end(), [&link](const Feed &feed) {
            return feed.link == link;
        });
        if (existing != m_feeds.end()) {
            next.append(*existing);
            existing->link.clear();
        } else {
            Feed feed;
            feed.link = link;
            feed.name = QUrl(link).host();
            feed.state = QStringLiteral("loading");
            next.append(feed);
        }
    }
    for (Feed &feed : m_feeds) {
        if (feed.link.isEmpty())
            continue;
        if (feed.reply)
            feed.reply->abort();
        lostEvents = lostEvents || feed.calendar;
    }
    m_feeds = next;
    m_links = normalized;

    for (Feed &feed : m_feeds) {
        if (!feed.calendar && !feed.reply)
            fetch(feed);
    }
    if (m_links.isEmpty())
        m_refresh.stop();
    else if (!m_refresh.isActive())
        m_refresh.start();

    Q_EMIT linksChanged();
    Q_EMIT feedsChanged();
    if (lostEvents) {
        ++m_revision;
        Q_EMIT eventsChanged();
    }
}

QVariantList CalendarFeeds::feeds() const
{
    QVariantList result;
    for (const Feed &feed : m_feeds) {
        result.append(QVariantMap{
            {QStringLiteral("link"), feed.link},
            {QStringLiteral("name"), feed.name},
            {QStringLiteral("color"), feed.color},
            {QStringLiteral("state"), feed.state},
            {QStringLiteral("message"), feed.message},
            {QStringLiteral("updated"), feed.updated},
        });
    }
    return result;
}

int CalendarFeeds::revision() const
{
    return m_revision;
}

void CalendarFeeds::setRefreshInterval(int milliseconds)
{
    m_refresh.setInterval(milliseconds);
}

void CalendarFeeds::refresh()
{
    for (Feed &feed : m_feeds)
        fetch(feed);
}

void CalendarFeeds::fetch(Feed &feed)
{
    if (feed.reply)
        feed.reply->abort();
    QNetworkRequest request{QUrl(feed.link)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(fetchTimeout);
    QNetworkReply *reply = m_network.get(request);
    feed.reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        finished(reply);
    });
}

CalendarFeeds::Feed *CalendarFeeds::feedFor(QNetworkReply *reply)
{
    for (Feed &feed : m_feeds) {
        if (feed.reply == reply)
            return &feed;
    }
    return nullptr;
}

void CalendarFeeds::finished(QNetworkReply *reply)
{
    reply->deleteLater();
    Feed *feed = feedFor(reply);
    if (!feed || reply->error() == QNetworkReply::OperationCanceledError)
        return;
    feed->reply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        feed->state = QStringLiteral("failed");
        feed->message = failureMessage(reply->error());
        Q_EMIT feedsChanged();
        return;
    }

    const QByteArray data = reply->readAll();
    auto calendar = KCalendarCore::MemoryCalendar::Ptr::create(QTimeZone::systemTimeZone());
    KCalendarCore::ICalFormat format;
    if (!data.contains("BEGIN:VCALENDAR") || !format.fromRawString(calendar, data)) {
        feed->state = QStringLiteral("failed");
        feed->message = i18nc("@info a linked calendar's status", "This link isn't a calendar");
        Q_EMIT feedsChanged();
        return;
    }

    feed->calendar = calendar;
    feed->name = calendarName(data, feed->link);
    feed->color = calendarColor(data);
    feed->state = QStringLiteral("ready");
    feed->message.clear();
    feed->updated = QDateTime::currentDateTime();
    ++m_revision;
    Q_EMIT feedsChanged();
    Q_EMIT eventsChanged();
}

QVariantMap CalendarFeeds::eventsForMonth(int year, int month) const
{
    const QTimeZone zone = QTimeZone::systemTimeZone();
    const QDate first(year, month, 1);
    if (!first.isValid())
        return {};
    const QDate last = first.addDays(first.daysInMonth() - 1);

    struct Entry {
        QDate day;
        QVariantMap event;
        bool allDay;
        QDateTime start;
    };
    QList<Entry> entries;

    for (const Feed &feed : m_feeds) {
        if (!feed.calendar)
            continue;
        const QString picked = m_colors.value(feed.link).toString();
        const QString calendarColor = picked.isEmpty() ? feed.color : picked;
        // A long event that began before the month still shows on its days.
        const KCalendarCore::Event::List events = feed.calendar->rawEvents(first.addDays(-62), last, zone, false);
        for (const KCalendarCore::Event::Ptr &event : events) {
            if (event->status() == KCalendarCore::Incidence::StatusCanceled)
                continue;
            const bool allDay = event->allDay();
            const QDateTime start = allDay ? event->dtStart() : event->dtStart().toTimeZone(zone);
            QDate endDay = allDay ? event->dateEnd() : event->dtEnd().toTimeZone(zone).date();
            // A timed event ending at midnight ends the day before.
            if (!allDay && event->dtEnd().toTimeZone(zone).time() == QTime(0, 0) && endDay > start.date())
                endDay = endDay.addDays(-1);
            const qint64 span = std::max<qint64>(0, start.date().daysTo(endDay));

            QList<QDateTime> occurrences;
            if (event->recurs()) {
                const QDateTime from(first.addDays(-span), QTime(0, 0), zone);
                const QDateTime to(last, QTime(23, 59, 59), zone);
                for (const QDateTime &time : event->recurrence()->timesInInterval(from, to)) {
                    // A moved or edited occurrence is its own event.
                    if (!feed.calendar->event(event->uid(), time))
                        occurrences.append(time);
                }
            } else {
                occurrences.append(event->dtStart());
            }

            const qint64 length = event->dtStart().secsTo(event->dtEnd());
            for (const QDateTime &occurrence : std::as_const(occurrences)) {
                const QDateTime local = allDay ? occurrence : occurrence.toTimeZone(zone);
                const QDate startDay = local.date();
                const QString key = event->uid() + QLatin1Char('|')
                    + occurrence.toUTC().toString(Qt::ISODate);
                for (qint64 offset = 0; offset <= span; ++offset) {
                    const QDate day = startDay.addDays(offset);
                    if (day < first || day > last)
                        continue;
                    // On the days after the first, a timed event is under way
                    // rather than starting, and reads like an all-day one.
                    const bool wholeDay = allDay || offset > 0;
                    entries.append(Entry{day,
                                         QVariantMap{
                                             {QStringLiteral("title"), event->summary()},
                                             {QStringLiteral("allDay"), wholeDay},
                                             {QStringLiteral("start"), wholeDay ? QDateTime() : local},
                                             {QStringLiteral("end"), wholeDay ? QDateTime() : local.addSecs(length)},
                                             {QStringLiteral("calendar"), feed.name},
                                             {QStringLiteral("color"), event->color().isEmpty() ? calendarColor : event->color()},
                                             {QStringLiteral("key"), key},
                                         },
                                         wholeDay,
                                         local});
                }
            }
        }
    }

    std::stable_sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        if (a.day != b.day)
            return a.day < b.day;
        if (a.allDay != b.allDay)
            return a.allDay;
        return a.start < b.start;
    });

    QVariantMap result;
    for (const Entry &entry : std::as_const(entries)) {
        const QString key = QString::number(entry.day.day());
        QVariantList list = result.value(key).toList();
        list.append(entry.event);
        result.insert(key, list);
    }
    return result;
}
