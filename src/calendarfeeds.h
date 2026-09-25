/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#pragma once

#include <KCalendarCore/MemoryCalendar>

#include <QDateTime>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariant>

class QNetworkReply;

// Reads the calendars a person links by pasting a private iCal address, the
// kind Google, iCloud and Outlook give out. Read-only: each link is fetched
// when set and again every refresh interval, and the last good copy is kept
// when a fetch fails.
class CalendarFeeds : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList links READ links WRITE setLinks NOTIFY linksChanged)
    // One entry per link: link, name, state ("loading", "ready" or "failed"),
    // message and updated.
    Q_PROPERTY(QVariantList feeds READ feeds NOTIFY feedsChanged)
    // Bumped whenever the events change, so bindings that read them re-read.
    Q_PROPERTY(int revision READ revision NOTIFY eventsChanged)

public:
    explicit CalendarFeeds(QObject *parent = nullptr);
    ~CalendarFeeds() override;

    QStringList links() const;
    void setLinks(const QStringList &links);
    QVariantList feeds() const;
    int revision() const;

    // A webcal:// address is the same feed over https. Anything that is not a
    // web address after trimming returns empty.
    Q_INVOKABLE static QString normalizedLink(const QString &link);

    // Events of every linked calendar that fall in the month, keyed by day of
    // the month as a string. Each is a map of title, allDay, start and
    // calendar, sorted all-day first and then by start.
    Q_INVOKABLE QVariantMap eventsForMonth(int year, int month) const;

    Q_INVOKABLE void refresh();

    void setRefreshInterval(int milliseconds);

Q_SIGNALS:
    void linksChanged();
    void feedsChanged();
    void eventsChanged();

private:
    struct Feed {
        QString link;
        QString name;
        QString state;
        QString message;
        QDateTime updated;
        KCalendarCore::MemoryCalendar::Ptr calendar;
        QPointer<QNetworkReply> reply;
    };

    void fetch(Feed &feed);
    void finished(QNetworkReply *reply);
    Feed *feedFor(QNetworkReply *reply);

    QStringList m_links;
    QList<Feed> m_feeds;
    QNetworkAccessManager m_network;
    QTimer m_refresh;
    int m_revision = 0;
};
