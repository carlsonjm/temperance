/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "keepawake.h"

#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QGuiApplication>
#include <QTimer>
#include <memory>

namespace
{
QString sleepService()
{
    return QLatin1String(KeepAwake::SleepService);
}

QDBusMessage call(const char *service, const char *path, const QString &method)
{
    // Each service's interface is named as the service is.
    return QDBusMessage::createMethodCall(QLatin1String(service), QLatin1String(path), QLatin1String(service), method);
}

QString applicationName()
{
    const QString name = QGuiApplication::desktopFileName();
    return name.isEmpty() ? QStringLiteral("co.goodinput.temperance") : name;
}
}

// The door's face on the bus: its scriptable members are the interface.
class KeepAwakeDoor : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "co.goodinput.Temperance.KeepAwake")

public:
    explicit KeepAwakeDoor(KeepAwake *awake)
        : QObject(awake)
        , m_awake(awake)
    {
        connect(awake, &KeepAwake::activeChanged, this, [this] { Q_EMIT Changed(Active(), Until()); });
    }

public Q_SLOTS:
    Q_SCRIPTABLE bool Active() const { return m_awake->active(); }
    Q_SCRIPTABLE QString Until() const
    {
        const QDateTime until = m_awake->until();
        return until.isValid() ? until.toOffsetFromUtc(until.offsetFromUtc()).toString(Qt::ISODate) : QString();
    }
    // True when it stands as asked. A time that is not one, or not ahead,
    // changes nothing.
    Q_SCRIPTABLE bool Set(bool active, const QString &until)
    {
        if (!active) {
            m_awake->setActive(false);
            return true;
        }
        if (!m_awake->available())
            return false;
        if (until.isEmpty())
            return m_awake->setActiveUntil({});
        const QDateTime end = QDateTime::fromString(until, Qt::ISODate);
        return end.isValid() && m_awake->setActiveUntil(end);
    }

Q_SIGNALS:
    Q_SCRIPTABLE void Changed(bool active, const QString &until);

private:
    KeepAwake *m_awake;
};

KeepAwake::KeepAwake(QObject *parent)
    : QObject(parent)
    , m_end(new QTimer(this))
{
    m_end->setSingleShot(true);
    connect(m_end, &QTimer::timeout, this, &KeepAwake::timeUp);
}

KeepAwake::~KeepAwake()
{
    // Whatever is still held ends with the panel's connection; letting go
    // here as well leaves nothing held by a keeper that is gone.
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (m_screenCookie) {
        QDBusMessage message = call(ScreenService, ScreenPath, QStringLiteral("UnInhibit"));
        message << *m_screenCookie;
        bus.call(message, QDBus::NoBlock);
    }
    if (m_sleepCookie) {
        QDBusMessage message = call(SleepService, SleepPath, QStringLiteral("UnInhibit"));
        message << *m_sleepCookie;
        bus.call(message, QDBus::NoBlock);
    }
}

void KeepAwake::start()
{
    if (m_watcher)
        return;
    QDBusConnection bus = QDBusConnection::sessionBus();
    m_watcher = new QDBusServiceWatcher(sleepService(), bus,
        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, [this] { setAvailable(true); });
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] { lost(); });
    setAvailable(bus.isConnected() && bus.interface() && bus.interface()->isServiceRegistered(sleepService()));
}

bool KeepAwake::openDoor()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    auto *door = new KeepAwakeDoor(this);
    if (!bus.registerObject(QLatin1String(DoorPath), door,
                            QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals)) {
        delete door;
        return false;
    }
    return bus.registerService(QLatin1String(DoorService));
}

QDateTime KeepAwake::until() const
{
    return m_until;
}

bool KeepAwake::timed() const
{
    return m_until.isValid();
}

bool KeepAwake::active() const
{
    return m_wanted;
}

bool KeepAwake::available() const
{
    return m_available;
}

void KeepAwake::toggle()
{
    setActive(!m_wanted);
}

// A tap, or anything else that names no end: on until sign-out, or off.
void KeepAwake::setActive(bool active)
{
    if (active && !m_available)
        return;
    const bool wasTimed = m_until.isValid();
    setUntil({});
    if (m_wanted == active) {
        if (wasTimed)
            Q_EMIT activeChanged();
        return;
    }
    m_wanted = active;
    Q_EMIT activeChanged();
    settle();
}

bool KeepAwake::setActiveUntil(const QDateTime &until)
{
    if (!until.isValid()) {
        setActive(true);
        return m_wanted;
    }
    if (!m_available || until <= QDateTime::currentDateTime())
        return false;
    const bool changed = !m_wanted || m_until != until;
    setUntil(until);
    m_wanted = true;
    if (changed)
        Q_EMIT activeChanged();
    settle();
    return true;
}

void KeepAwake::setUntil(const QDateTime &until)
{
    m_until = until;
    if (!until.isValid()) {
        m_end->stop();
        return;
    }
    // A timer counts in an int of milliseconds: a far end is looked at
    // again on the way.
    const qint64 ahead = QDateTime::currentDateTime().msecsTo(until);
    m_end->start(int(qBound(qint64(0), ahead + 1, qint64(24 * 60 * 60 * 1000))));
}

void KeepAwake::timeUp()
{
    if (!m_until.isValid())
        return;
    if (m_until > QDateTime::currentDateTime()) {
        setUntil(m_until);
        return;
    }
    setActive(false);
}

// Brings what is held in line with what was last asked. One request is in
// flight at a time; taps made meanwhile are settled when it answers.
void KeepAwake::settle()
{
    if (m_busy)
        return;
    if (m_wanted && !m_sleepCookie)
        hold();
    else if (!m_wanted && (m_sleepCookie || m_screenCookie))
        release();
}

void KeepAwake::hold()
{
    m_busy = true;
    const QString reason = i18n("Keep awake is on in Control Center");
    QDBusMessage message = call(SleepService, SleepPath, QStringLiteral("Inhibit"));
    message << applicationName() << reason;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, reason](QDBusPendingCallWatcher *sleepWatcher) {
        sleepWatcher->deleteLater();
        QDBusPendingReply<uint> sleepReply = *sleepWatcher;
        if (sleepReply.isError()) {
            m_busy = false;
            if (m_wanted) {
                m_wanted = false;
                setUntil({});
                Q_EMIT activeChanged();
                Q_EMIT failed();
            }
            settle();
            return;
        }
        m_sleepCookie = sleepReply.value();

        // The screen's own hold keeps it from dimming and locking. Plasma's
        // page asks for it the same way, and it is not a reason to refuse:
        // without a screen locker there is nothing to hold.
        QDBusMessage screen = call(ScreenService, ScreenPath, QStringLiteral("Inhibit"));
        screen << applicationName() << reason;
        auto *screenWatcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(screen), this);
        connect(screenWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
            watcher->deleteLater();
            QDBusPendingReply<uint> screenReply = *watcher;
            if (!screenReply.isError())
                m_screenCookie = screenReply.value();
            m_busy = false;
            settle();
        });
    });
}

void KeepAwake::release()
{
    m_busy = true;
    auto pending = std::make_shared<int>(0);
    const auto finish = [this, pending] {
        if (--*pending > 0)
            return;
        m_busy = false;
        settle();
    };
    const auto send = [this, pending, finish](const char *service, const char *path, std::optional<uint> &cookie) {
        if (!cookie)
            return;
        QDBusMessage message = call(service, path, QStringLiteral("UnInhibit"));
        message << *cookie;
        // Taken as let go whatever the answer: a refused cookie is one
        // Plasma no longer holds, and keeping it would leave the switch stuck.
        cookie.reset();
        ++*pending;
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [finish](QDBusPendingCallWatcher *done) {
            done->deleteLater();
            finish();
        });
    };
    ++*pending;
    send(ScreenService, ScreenPath, m_screenCookie);
    send(SleepService, SleepPath, m_sleepCookie);
    finish();
}

void KeepAwake::setAvailable(bool available)
{
    if (m_available == available)
        return;
    m_available = available;
    Q_EMIT availableChanged();
}

// Power management went away, and what it held went with it: the switch
// reads off, and a fresh tap asks again once it is back.
void KeepAwake::lost()
{
    m_sleepCookie.reset();
    // The screen's hold is kept elsewhere and outlives it; let it go too.
    if (m_screenCookie) {
        QDBusMessage message = call(ScreenService, ScreenPath, QStringLiteral("UnInhibit"));
        message << *m_screenCookie;
        QDBusConnection::sessionBus().call(message, QDBus::NoBlock);
        m_screenCookie.reset();
    }
    if (m_wanted) {
        m_wanted = false;
        setUntil({});
        Q_EMIT activeChanged();
    }
    setAvailable(false);
}

#include "keepawake.moc"
