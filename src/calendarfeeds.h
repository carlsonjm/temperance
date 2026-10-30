/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#pragma once

#include <KCalendarCore/MemoryCalendar>

#include <QDateTime>
#include <QFileSystemWatcher>
#include <QHash>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QThreadPool>
#include <QTimer>
#include <QVariant>

class QNetworkReply;

// Reads the calendars a person links by pasting a private iCal address, the
// kind Google, iCloud and Outlook give out. Read-only: each link is fetched
// when set and again every refresh interval, and the last good copy is kept
// when a fetch fails.
//
// It also reads a local folder, one iCalendar file per event, which other
// programs on the computer write into, as a mail program adds an invitation
// it accepted. The folder is read again whenever its files change.
class CalendarFeeds : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList links READ links WRITE setLinks NOTIFY linksChanged)
    // The colour the person picked for a calendar, keyed by its link. It wins
    // over the colour the feed gives its calendar, not over an event's own.
    Q_PROPERTY(QVariantMap colors READ colors WRITE setColors NOTIFY colorsChanged)
    // One entry per link: link, name, color (the feed's own), state
    // ("loading", "ready" or "failed"), message and updated.
    Q_PROPERTY(QVariantList feeds READ feeds NOTIFY feedsChanged)
    // Bumped whenever the events change, so bindings that read them re-read.
    Q_PROPERTY(int revision READ revision NOTIFY eventsChanged)

public:
    explicit CalendarFeeds(QObject *parent = nullptr);
    ~CalendarFeeds() override;

    QStringList links() const;
    void setLinks(const QStringList &links);
    QVariantMap colors() const;
    void setColors(const QVariantMap &colors);
    QVariantList feeds() const;
    int revision() const;

    // A webcal:// or http:// address is the same feed over https; a local
    // file stays as it is. Anything else after trimming returns empty.
    Q_INVOKABLE static QString normalizedLink(const QString &link);

    // Events of every linked calendar and of the local folder that fall in the
    // month, keyed by day of the month as a string. Each is a map of title,
    // allDay, continuing, tentative, start, end, calendar, color and key,
    // sorted all-day first and then by
    // start. A timed event on a day after the one it starts is allDay and
    // continuing, and keeps its real start and end. The key
    // names one occurrence, so it survives a re-read of the feed. The colour is
    // the event's own, else the one the person picked for its calendar, else
    // the feed's, else empty: iCloud's feeds carry one, Google's and Outlook's
    // do not.
    // Worked out once per month and kept until the events change, so the
    // ticker's tick each minute and the calendar card cost a lookup.
    Q_INVOKABLE QVariantMap eventsForMonth(int year, int month) const;

    // The events of one day, as eventsForMonth() lists them.
    Q_INVOKABLE QVariantList eventsForDay(int year, int month, int day) const;

    Q_INVOKABLE void refresh();

    void setRefreshInterval(int milliseconds);

    // The local folder: each file in it whose name ends .ics and does not
    // start with a dot is one event, or one event and its changed
    // occurrences. Empty reads no folder, the default. A folder that does not
    // exist yet is read once it is made.
    QString localFolder() const;
    void setLocalFolder(const QString &path);
    // Where the local folder is kept: temperance/calendar under the user's
    // data folder, normally ~/.local/share/temperance/calendar.
    static QString defaultLocalFolder();

Q_SIGNALS:
    void linksChanged();
    void colorsChanged();
    void feedsChanged();
    void eventsChanged();

private:
    struct Feed {
        QString link;
        QString name;
        QString color;
        QString state;
        QString message;
        QDateTime updated;
        KCalendarCore::MemoryCalendar::Ptr calendar;
        QPointer<QNetworkReply> reply;
        // The read being parsed; a later one supersedes it.
        quint64 parse = 0;
    };

    void fetch(Feed &feed);
    void finished(QNetworkReply *reply);
    void parsed(const QString &link, quint64 parse, const KCalendarCore::MemoryCalendar::Ptr &calendar,
                const QString &name, const QString &color);
    Feed *feedFor(QNetworkReply *reply);
    QVariantMap computeMonth(int year, int month) const;
    void watchLocalFolder();
    void readLocalFolder();

    QStringList m_links;
    QVariantMap m_colors;
    QList<Feed> m_feeds;
    QNetworkAccessManager m_network;
    QTimer m_refresh;
    int m_revision = 0;
    quint64 m_parseSerial = 0;
    // Feeds are parsed away from the panel's own thread, one at a time.
    QThreadPool m_parser;
    QString m_localFolder;
    KCalendarCore::MemoryCalendar::Ptr m_local;
    QFileSystemWatcher m_localWatcher;
    // Writers replace a file in several steps; the folder is read once they settle.
    QTimer m_localSettle;
    quint64 m_localRead = 0;
    mutable QHash<QString, QVariantMap> m_months;
    mutable int m_monthsRevision = -1;
};
