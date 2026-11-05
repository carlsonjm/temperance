// SPDX-License-Identifier: LGPL-2.0-or-later
#include <QApplication>
#include <algorithm>
#include <functional>
#include <QAbstractListModel>
#include <QDBusConnection>
#include <notification.h>
#include <notifications.h>
#include <server.h>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <KLocalizedContext>

class SystemActionReceiver : public QObject {
    Q_OBJECT
public:
    uint notificationId = 0;
    QString actionId;
public Q_SLOTS:
    void actionInvoked(uint id, const QString &action) {
        notificationId = id;
        actionId = action;
    }
};

class TestNotifications : public NotificationManager::Notifications {
public:
    using NotificationManager::Notifications::classBegin;
    using NotificationManager::Notifications::componentComplete;
};

// Typed QStringList roles and QModelIndex calls match the installed API boundary.
class HistoryFixture : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int unreadNotificationsCount READ rowCount NOTIFY countChanged)
public:
    using Roles = NotificationManager::Notifications;
    QList<QVariantMap> rows;
    int defaultRow = -1, closedRow = -1, actionRow = -1, expandedRow = -1;
    int defaultCalls = 0, actionCalls = 0, closeCalls = 0, expandedRole = -1;
    QString actionName;
    bool expanded = false;
    QHash<int, QByteArray> roleNames() const override {
        return {{Roles::IsGroupRole, "isGroup"}, {Roles::IsInGroupRole, "isInGroup"},
            {Roles::IsGroupExpandedRole, "isGroupExpanded"}, {Roles::GroupChildrenCountRole, "groupChildrenCount"},
            {Roles::SummaryRole, "summary"}, {Roles::BodyRole, "body"},
            {Roles::ApplicationNameRole, "applicationName"}, {Roles::ApplicationIconNameRole, "applicationIconName"},
            {Roles::DesktopEntryRole, "desktopEntry"}, {Roles::HasDefaultActionRole, "hasDefaultAction"},
            {Roles::ActionNamesRole, "actionNames"}, {Roles::ActionLabelsRole, "actionLabels"},
            {Roles::IdRole, "notificationId"}};
    }
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : rows.size(); }
    QVariant data(const QModelIndex &idx, int role) const override {
        if (!idx.isValid() || idx.row() >= rows.size()) return {};
        return rows[idx.row()].value(QString::fromUtf8(roleNames().value(role)));
    }
    Q_INVOKABLE void invokeDefaultAction(const QModelIndex &idx) { defaultRow = idx.row(); ++defaultCalls; }
    Q_INVOKABLE void invokeAction(const QModelIndex &idx, const QString &name) {
        actionRow = idx.row(); actionName = name; ++actionCalls;
    }
    bool closeRemoves = false;
    Q_INVOKABLE void close(const QModelIndex &idx) {
        closedRow = idx.row(); ++closeCalls;
        if (closeRemoves && idx.row() < rows.size()) removeAt(idx.row());
    }
    bool setData(const QModelIndex &idx, const QVariant &value, int role) override {
        expandedRow = idx.row(); expandedRole = role; expanded = value.toBool();
        rows[idx.row()][QStringLiteral("isGroupExpanded")] = value;
        Q_EMIT dataChanged(idx, idx, {Roles::IsGroupExpandedRole});
        return true;
    }
    void append(bool grouped, bool group = false, bool actionable = false, int at = -1) {
        const int row = at < 0 ? rows.size() : at;
        beginInsertRows({}, row, row);
        rows.insert(row, make(grouped, group, actionable));
        endInsertRows();
        Q_EMIT countChanged();
    }
    // A row as the history holds it; the churn test names and sizes its own.
    static QVariantMap make(bool grouped, bool group = false, bool actionable = false) {
        return {{QStringLiteral("isGroup"), group}, {QStringLiteral("isInGroup"), grouped},
            {QStringLiteral("isGroupExpanded"), true}, {QStringLiteral("groupChildrenCount"), group ? 2 : 0},
            {QStringLiteral("summary"), QStringLiteral("Test alert")},
            {QStringLiteral("body"), QString(QStringLiteral("<html><b>Long formatted body</b><br>")
                + QStringLiteral("message ").repeated(150) + QStringLiteral("</html>"))},
            {QStringLiteral("applicationName"), QStringLiteral("Fixture")},
            {QStringLiteral("applicationIconName"), QString()}, {QStringLiteral("desktopEntry"), QStringLiteral("fixture")},
            {QStringLiteral("hasDefaultAction"), true},
            {QStringLiteral("actionNames"), actionable
                ? QStringList{QStringLiteral("accept"), QStringLiteral("reject"),
                    QStringLiteral("default"), QStringLiteral("unlabelled")}
                : QStringList{}},
            {QStringLiteral("actionLabels"), actionable
                ? QStringList{QStringLiteral("Accept"), QStringLiteral("Reject"), QStringLiteral("Open")}
                : QStringList{}}};
    }
    static QVariantMap entry(const QString &application, const QString &summary, int bodyWords,
            bool grouped = false, bool group = false) {
        QVariantMap row = make(grouped, group);
        row[QStringLiteral("applicationName")] = application;
        row[QStringLiteral("summary")] = summary;
        row[QStringLiteral("notificationId")] = summary;
        row[QStringLiteral("body")] = QStringLiteral("word ").repeated(bodyWords).trimmed();
        row[QStringLiteral("isGroupExpanded")] = false;
        row[QStringLiteral("groupChildrenCount")] = group ? 3 : 0;
        return row;
    }
    void insertAt(int row, const QVariantMap &value) {
        beginInsertRows({}, row, row);
        rows.insert(row, value);
        endInsertRows();
        Q_EMIT countChanged();
    }
    void removeAt(int row) {
        beginRemoveRows({}, row, row);
        rows.removeAt(row);
        endRemoveRows();
        Q_EMIT countChanged();
    }
    void change(int row, const QString &key, const QVariant &value) {
        rows[row][key] = value;
        Q_EMIT dataChanged(index(row, 0), index(row, 0));
    }
    // What a sorting or grouping proxy sends when it sorts again or a group
    // opens: one layoutChanged in which rows move, and some come and go.
    void relayout(const QList<QVariantMap> &next) {
        Q_EMIT layoutAboutToBeChanged();
        rows = next;
        Q_EMIT layoutChanged();
        Q_EMIT countChanged();
    }
    void reset(const QList<QVariantMap> &next) {
        beginResetModel();
        rows = next;
        endResetModel();
        Q_EMIT countChanged();
    }
Q_SIGNALS:
    void countChanged();
};

class NotificationInteractionTest : public QObject {
    Q_OBJECT
    static QQuickItem *findItem(QQuickItem *root, const QString &name) {
        if (root->objectName() == name) return root;
        for (auto *child : root->childItems()) {
            if (auto *found = findItem(child, name)) return found;
        }
        return nullptr;
    }
private Q_SLOTS:
    void snoozeUsesSystemNotificationAction() {
        auto &server = NotificationManager::Server::self();

        TestNotifications notifications;
        notifications.classBegin();
        notifications.componentComplete();

        SystemActionReceiver receiver;
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.connect(QString(), QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("ActionInvoked"),
            &receiver, SLOT(actionInvoked(uint,QString))));

        NotificationManager::Notification notification;
        notification.setDBusService(bus.baseService());
        notification.setApplicationName(QStringLiteral("Temperance action probe"));
        notification.setSummary(QStringLiteral("System action probe"));
        notification.setBody(QStringLiteral("Verify producer-owned Snooze"));
        notification.setActions({QStringLiteral("snooze"), QStringLiteral("Snooze")});
        const uint id = server.add(notification);

        QModelIndex actionIndex;
        QTRY_VERIFY_WITH_TIMEOUT(notifications.rowCount() > 0, 2000);
        for (int row = 0; row < notifications.rowCount(); ++row) {
            const QModelIndex candidate = notifications.index(row, 0);
            if (notifications.data(candidate, NotificationManager::Notifications::IdRole).toUInt() == id) {
                actionIndex = candidate;
                break;
            }
        }
        QVERIFY(actionIndex.isValid());
        QCOMPARE(notifications.data(actionIndex,
            NotificationManager::Notifications::ActionNamesRole).toStringList(),
            QStringList{QStringLiteral("snooze")});
        QCOMPARE(notifications.data(actionIndex,
            NotificationManager::Notifications::ActionLabelsRole).toStringList(),
            QStringList{QStringLiteral("Snooze")});

        notifications.invokeAction(actionIndex, QStringLiteral("snooze"));
        QTRY_COMPARE_WITH_TIMEOUT(receiver.notificationId, id, 2000);
        QCOMPARE(receiver.actionId, QStringLiteral("snooze"));
    }

    void history_data() {
        QTest::addColumn<bool>("grouped");
        QTest::addColumn<bool>("touch");
        QTest::newRow("standalone-mouse") << false << false;
        QTest::newRow("group-child-mouse") << true << false;
        QTest::newRow("standalone-touch") << false << true;
        QTest::newRow("group-child-touch") << true << true;
    }
    void history() {
        QFETCH(bool, grouped);
        QFETCH(bool, touch);
        QQmlEngine engine;
        engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
        HistoryFixture model;
        model.append(grouped);
        engine.globalObject().setProperty(QStringLiteral("fixtureModel"), engine.newQObject(&model));
        QQmlComponent pageComponent(&engine, QUrl::fromLocalFile(QStringLiteral(HISTORY_QML)));
        QScopedPointer<QObject> page(pageComponent.createWithInitialProperties({
            {QStringLiteral("notificationModel"), QVariant::fromValue(&model)},
            {QStringLiteral("clearHistory"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("resolveApplicationIcon"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){return '';})")))},
            {QStringLiteral("launchApplication"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(id){fixtureModel.invokeAction(fixtureModel.index(0,0), id);})")))},
            {QStringLiteral("demoNotificationVisible"), false}, {QStringLiteral("maximumHeight"), 600}
        }));
        QVERIFY2(page, qPrintable(pageComponent.errorString()));
        auto *item = qobject_cast<QQuickItem *>(page.data());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(430, 600);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(430, 600));
        window.show();
        QQuickItem *row = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((row = findItem(item, QStringLiteral("notificationRow0"))), 2000);
        auto *more = row->findChild<QQuickItem *>("notificationReadMore");
        auto *body = row->findChild<QQuickItem *>("notificationBody");
        auto *summary = row->findChild<QQuickItem *>("notificationSummary");
        auto *application = row->findChild<QQuickItem *>("notificationApplication");
        auto *header = row->findChild<QQuickItem *>("notificationHeader");
        auto *separator = row->findChild<QQuickItem *>("notificationHeaderSeparator");
        auto *card = row->findChild<QQuickItem *>("notificationCard");
        auto *cardContent = row->findChild<QQuickItem *>("notificationCardContent");
        QVERIFY(more && body && summary && application && header && separator && card && cardContent);
        QTRY_VERIFY(header->height() > 0 && body->height() > 0);
        const QRectF headerRect = header->mapRectToScene(header->boundingRect());
        const QRectF bodyRect = body->mapRectToScene(body->boundingRect());
        QVERIFY2(bodyRect.top() >= headerRect.bottom(), "Notification body must sit directly below the header row");
        QVERIFY2(card->height() - cardContent->implicitHeight() <= 16,
            "Notification card vertical padding must stay compact");
        QCOMPARE(application->isVisible(), !grouped);
        QCOMPARE(separator->isVisible(), !grouped);
        if (!grouped) {
            const QRectF applicationRect = application->mapRectToScene(application->boundingRect());
            const QRectF summaryRect = summary->mapRectToScene(summary->boundingRect());
            QVERIFY2(qAbs(applicationRect.center().y() - summaryRect.center().y()) < 1.0,
                "Application and title must share one header row");
        }
        QTRY_VERIFY(more->isVisible());
        QVERIFY(body->property("truncated").toBool());
        auto *device = touch ? QTest::createTouchDevice() : nullptr;
        const auto click = [&](QQuickItem *target) {
            const auto point = target->mapToScene(QPointF(target->width()/2, target->height()/2)).toPoint();
            if (touch) {
                QTest::touchEvent(&window, device).press(0, point, &window);
                QTest::touchEvent(&window, device).release(0, point, &window);
            } else {
                QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, point);
            }
        };
        click(more);
        QTRY_VERIFY(row->property("detailsExpanded").toBool());
        QCOMPARE(model.defaultRow, -1);
        QTRY_VERIFY(!body->property("truncated").toBool());
        row->setProperty("detailsExpanded", false);
        QTest::qWait(50);
        click(summary);
        QTRY_COMPARE(model.defaultRow, 0);
        row->setProperty("hasDefaultAction", false);
        click(summary);
        QTRY_COMPARE(model.actionName, QStringLiteral("fixture"));
        QCOMPARE(model.defaultCalls, 1);
        click(findItem(row, QStringLiteral("notificationDismiss")));
        QTRY_COMPARE(model.closedRow, 0);
        QCOMPARE(model.defaultCalls, 1);
        item->setParentItem(nullptr);
    }
    // When another program holds the notification service, the history says
    // so, and who, instead of claiming all is caught up.
    void unavailableService() {
        QQmlEngine engine;
        engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
        HistoryFixture model;
        QQmlComponent pageComponent(&engine, QUrl::fromLocalFile(QStringLiteral(HISTORY_QML)));
        QScopedPointer<QObject> page(pageComponent.createWithInitialProperties({
            {QStringLiteral("notificationModel"), QVariant::fromValue(&model)},
            {QStringLiteral("clearHistory"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("resolveApplicationIcon"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){return '';})")))},
            {QStringLiteral("launchApplication"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("demoNotificationVisible"), false}, {QStringLiteral("maximumHeight"), 600},
            {QStringLiteral("serviceAvailable"), false}, {QStringLiteral("serviceOwner"), QStringLiteral("Example Daemon")}
        }));
        QVERIFY2(page, qPrintable(pageComponent.errorString()));
        auto *item = qobject_cast<QQuickItem *>(page.data());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(430, 600);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(430, 600));
        window.show();
        auto *title = findItem(item, QStringLiteral("notificationEmptyTitle"));
        auto *detail = findItem(item, QStringLiteral("notificationEmptyDetail"));
        QVERIFY(title && detail);
        QVERIFY(title->isVisible() && detail->isVisible());
        QVERIFY(title->property("text").toString() != QStringLiteral("All caught up"));
        QVERIFY(detail->property("text").toString().contains(QStringLiteral("Example Daemon")));
        page->setProperty("serviceAvailable", true);
        QCOMPARE(title->property("text").toString(), QStringLiteral("All caught up"));
        QVERIFY(!detail->isVisible());
    }

    void layoutFallbacks() {
        QQmlEngine engine;
        engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
        HistoryFixture model;
        model.append(false);
        model.append(false);
        model.append(false);
        model.rows[0][QStringLiteral("applicationName")] = QString();
        model.rows[1][QStringLiteral("summary")] = QString();
        model.rows[2][QStringLiteral("applicationName")] = QString();
        model.rows[2][QStringLiteral("summary")] = QString();
        for (auto &row : model.rows) row[QStringLiteral("body")] = QStringLiteral("Body only");
        QQmlComponent pageComponent(&engine, QUrl::fromLocalFile(QStringLiteral(HISTORY_QML)));
        QScopedPointer<QObject> page(pageComponent.createWithInitialProperties({
            {QStringLiteral("notificationModel"), QVariant::fromValue(&model)},
            {QStringLiteral("clearHistory"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("resolveApplicationIcon"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){return '';})")))},
            {QStringLiteral("launchApplication"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("demoNotificationVisible"), false}, {QStringLiteral("maximumHeight"), 600}
        }));
        QVERIFY2(page, qPrintable(pageComponent.errorString()));
        auto *item = qobject_cast<QQuickItem *>(page.data());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(430, 600);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(430, 600));
        window.show();
        for (int index = 0; index < 3; ++index) {
            QQuickItem *row = nullptr;
            QTRY_VERIFY((row = findItem(item, QStringLiteral("notificationRow%1").arg(index))));
            auto *application = row->findChild<QQuickItem *>("notificationApplication");
            auto *summary = row->findChild<QQuickItem *>("notificationSummary");
            auto *separator = row->findChild<QQuickItem *>("notificationHeaderSeparator");
            auto *header = row->findChild<QQuickItem *>("notificationHeader");
            auto *body = row->findChild<QQuickItem *>("notificationBody");
            QVERIFY(application && summary && separator && header && body);
            QVERIFY(!separator->isVisible());
            QCOMPARE(header->isVisible(), index != 2);
            QTRY_VERIFY(body->isVisible() && body->height() > 0);
        }
        item->setParentItem(nullptr);
    }
    void actions_data() {
        QTest::addColumn<bool>("touch");
        QTest::newRow("mouse") << false;
        QTest::newRow("touch") << true;
    }
    void actions() {
        QFETCH(bool, touch);
        QQmlEngine engine;
        engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
        HistoryFixture model;
        model.append(false, true);
        model.append(true);
        model.append(false);
        model.rows[1][QStringLiteral("body")] = QStringLiteral("One notification with producer actions");
        model.rows[1][QStringLiteral("actionNames")] = QStringList{QStringLiteral("accept"), QStringLiteral("reject"),
            QStringLiteral("default"), QStringLiteral("unlabelled")};
        model.rows[1][QStringLiteral("actionLabels")] = QStringList{QStringLiteral("Accept"), QStringLiteral("Reject"),
            QStringLiteral("Open")};
        QQmlComponent pageComponent(&engine, QUrl::fromLocalFile(QStringLiteral(HISTORY_QML)));
        QScopedPointer<QObject> page(pageComponent.createWithInitialProperties({
            {QStringLiteral("notificationModel"), QVariant::fromValue(&model)},
            {QStringLiteral("clearHistory"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("resolveApplicationIcon"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){return '';})")))},
            {QStringLiteral("launchApplication"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("demoNotificationVisible"), false}, {QStringLiteral("maximumHeight"), 600}
        }));
        QVERIFY2(page, qPrintable(pageComponent.errorString()));
        auto *item = qobject_cast<QQuickItem *>(page.data());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(430, 600);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(430, 600));
        window.show();
        // The rows exist as soon as the page does; let it lay them out
        // before reading where the buttons are.
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(50);
        QQuickItem *row = nullptr, *group = nullptr;
        QTRY_VERIFY((row = findItem(item, QStringLiteral("notificationRow1"))));
        QTRY_VERIFY((group = findItem(item, QStringLiteral("notificationRow0"))));
        QQuickItem *accept = nullptr;
        QTRY_VERIFY((accept = findItem(row, QStringLiteral("notificationAction-accept"))));
        auto *reject = findItem(row, QStringLiteral("notificationAction-reject"));
        auto *dismiss = findItem(row, QStringLiteral("notificationDismiss"));
        QVERIFY(reject && dismiss);
        QVERIFY(!findItem(row, QStringLiteral("notificationAction-default")));
        QVERIFY(!findItem(row, QStringLiteral("notificationAction-unlabelled")));
        QCOMPARE(accept->property("text").toString(), QStringLiteral("Accept"));
        QTRY_VERIFY(accept->isVisible() && accept->height() >= 44 && accept->width() >= 44);
        auto *actionBackground = accept->findChild<QQuickItem *>("notificationActionBackground");
        QVERIFY(actionBackground);
        QTRY_COMPARE(actionBackground->height(), 30.0);
        QVERIFY(actionBackground->height() < accept->height());
        QCOMPARE(accept->property("visualOutlineWidth").toReal(), 1.0);
        // The outline is the page's text: the theme's own on a light ground,
        // and Ghost White on a dark one whatever the theme's text is.
        auto *tone = item->findChild<QObject *>(QStringLiteral("temperanceStatusColors"), Qt::FindDirectChildrenOnly);
        QVERIFY(tone);
        const auto makeTheme = [&](const QString &ground, const QString &text) {
            QQmlComponent themeComponent(&engine);
            themeComponent.setData(QStringLiteral("import QtQuick\nQtObject { property color backgroundColor: \"%1\"; "
                "property color textColor: \"%2\"; property color negativeTextColor: \"#DA4453\" }")
                .arg(ground, text).toUtf8(), QUrl());
            return themeComponent.create();
        };
        QScopedPointer<QObject> lightTheme(makeTheme(QStringLiteral("#F2F2F2"), QStringLiteral("#102729")));
        QScopedPointer<QObject> darkTheme(makeTheme(QStringLiteral("#141414"), QStringLiteral("#E0E0E0")));
        QVERIFY(lightTheme && darkTheme);
        tone->setProperty("theme", QVariant::fromValue(lightTheme.data()));
        QCOMPARE(accept->property("visualOutline").value<QColor>(), QColor(QStringLiteral("#102729")));
        tone->setProperty("theme", QVariant::fromValue(darkTheme.data()));
        QCOMPARE(accept->property("visualOutline").value<QColor>(), QColor(QStringLiteral("#F8F8FF")));
        QCOMPARE(accept->property("visualFill").value<QColor>().alpha(), 0);
        // A plain dark style draws the suite's greys exactly; a tinted one
        // leans them toward its colour, as far as its ground leans.
        QCOMPARE(tone->property("surface").value<QColor>(), QColor(QStringLiteral("#141414")));
        QScopedPointer<QObject> tintedTheme(makeTheme(QStringLiteral("#10151A"), QStringLiteral("#E0E0E0")));
        QVERIFY(tintedTheme);
        tone->setProperty("theme", QVariant::fromValue(tintedTheme.data()));
        const auto tintedSurface = tone->property("surface").value<QColor>();
        QCOMPARE(tintedSurface, QColor(QStringLiteral("#0F1419")));
        QVERIFY(tone->property("control").value<QColor>().blue() > tone->property("control").value<QColor>().red());
        tone->setProperty("theme", QVariant::fromValue(darkTheme.data()));
        QCOMPARE(accept->property("leftPadding").toReal(), 14.0);
        QCOMPARE(accept->property("rightPadding").toReal(), 14.0);
        QTRY_COMPARE(actionBackground->width(), accept->width());
        auto *device = touch ? QTest::createTouchDevice() : nullptr;
        const auto click = [&](QQuickItem *target) {
            const auto point = target->mapToScene(QPointF(target->width()/2, target->height()/2)).toPoint();
            if (touch) {
                QTest::touchEvent(&window, device).press(0, point, &window);
                QTest::touchEvent(&window, device).release(0, point, &window);
            } else QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, point);
        };
        if (!touch) {
            const auto hoverPoint = accept->mapToScene(
                QPointF(accept->width() / 2, accept->height() / 2)).toPoint();
            QTest::mouseMove(&window, hoverPoint);
            QTRY_VERIFY(accept->property("hovered").toBool());
            QTRY_VERIFY(accept->property("visualFill").value<QColor>().alpha() > 0);
        }
        click(accept);
        QTRY_COMPARE(model.actionCalls, 1);
        QCOMPARE(model.actionRow, 1);
        QCOMPARE(model.actionName, QStringLiteral("accept"));
        QCOMPARE(model.defaultCalls, 0);
        click(reject);
        QTRY_COMPARE(model.actionCalls, 2);
        QCOMPARE(model.actionName, QStringLiteral("reject"));
        QCOMPARE(model.defaultCalls, 0);
        click(dismiss);
        QTRY_COMPARE(model.closeCalls, 1);
        QCOMPARE(model.closedRow, 1); // Individual child, never the group index.
        QCOMPARE(model.defaultCalls, 0);
        accept->forceActiveFocus();
        QTRY_COMPARE(accept->property("visualOpacity").toReal(), 1.0);
        QTest::keyClick(&window, Qt::Key_Space);
        QTRY_COMPARE(model.actionCalls, 3);
        QCOMPARE(model.defaultCalls, 0);
        QTest::keyClick(&window, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(model.actionCalls, 4, 1000);
        QCOMPARE(model.defaultCalls, 0);
        click(findItem(row, QStringLiteral("notificationSummary")));
        QTRY_COMPARE(model.defaultCalls, 1);
        QCOMPARE(model.defaultRow, 1);
        QCOMPARE(model.actionCalls, 4);
        auto *expand = findItem(group, QStringLiteral("notificationExpandGroup"));
        auto *clear = findItem(group, QStringLiteral("notificationClearGroup"));
        QVERIFY(expand && clear);
        QVERIFY(!findItem(group, QStringLiteral("notificationDismiss"))->isVisible());
        click(expand);
        QTRY_COMPARE(model.expandedRow, 0);
        QVERIFY(!model.expanded);
        QCOMPARE(model.expandedRole, int(HistoryFixture::Roles::IsGroupExpandedRole));
        click(expand);
        QTRY_VERIFY(model.expanded);
        click(clear);
        QTRY_COMPARE(model.closeCalls, 2);
        QCOMPARE(model.closedRow, 0); // Group clear keeps the group API semantics.
        QCOMPARE(model.defaultCalls, 1);
        item->setParentItem(nullptr);
    }

    // A card keeps its header when the bell closes and opens again, and a
    // long card arriving at the top pushes the cards below past its whole
    // height, including the Show more it gains once its text is measured.
    void arrivalGeometry() {
        QQmlEngine engine;
        engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
        HistoryFixture model;
        model.append(false);
        QQmlComponent pageComponent(&engine, QUrl::fromLocalFile(QStringLiteral(HISTORY_QML)));
        QScopedPointer<QObject> page(pageComponent.createWithInitialProperties({
            {QStringLiteral("notificationModel"), QVariant::fromValue(&model)},
            {QStringLiteral("clearHistory"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("resolveApplicationIcon"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){return '';})")))},
            {QStringLiteral("launchApplication"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("demoNotificationVisible"), false}, {QStringLiteral("maximumHeight"), 900}
        }));
        QVERIFY2(page, qPrintable(pageComponent.errorString()));
        auto *item = qobject_cast<QQuickItem *>(page.data());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(430, 900);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(430, 900));
        window.show();
        QQuickItem *closedRow = nullptr;
        QTRY_VERIFY((closedRow = findItem(item, QStringLiteral("notificationRow0"))));
        item->setVisible(false);
        QTest::qWait(50);
        item->setVisible(true);
        auto *closedSummary = closedRow->findChild<QQuickItem *>("notificationSummary");
        auto *closedApplication = closedRow->findChild<QQuickItem *>("notificationApplication");
        QVERIFY(closedSummary && closedApplication);
        QTRY_VERIFY2(closedSummary->isVisible() && closedApplication->isVisible(),
            "A card must show its title again when the bell reopens");
        QCOMPARE(closedSummary->property("text").toString(), QStringLiteral("Test alert"));

        model.append(false, false, false, 0);
        QQuickItem *arrived = nullptr;
        QTRY_VERIFY((arrived = findItem(item, QStringLiteral("notificationRow0"))) && arrived != closedRow);
        QTRY_VERIFY(arrived->findChild<QQuickItem *>("notificationReadMore")->isVisible());
        QTest::qWait(400);
        const auto *arrivedCard = arrived->findChild<QQuickItem *>("notificationCard");
        const QRectF arrivedRect = arrivedCard->mapRectToItem(item, arrivedCard->boundingRect());
        auto *dismiss = findItem(arrived, QStringLiteral("notificationDismiss"));
        const QRectF dismissRect = dismiss->mapRectToItem(item, dismiss->boundingRect());
        QVERIFY2(dismissRect.bottom() <= arrivedRect.bottom() + 0.5, "Dismiss must stay inside its own card");
        const QRectF belowRect = closedRow->mapRectToItem(item, closedRow->boundingRect());
        QVERIFY2(belowRect.top() >= arrivedRect.bottom(),
            "The card below a new arrival must start after the arrival's whole card");
        item->setParentItem(nullptr);
    }

    void repeatedActionGeometry() {
        QQmlEngine engine;
        engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
        HistoryFixture model;
        model.append(false, true);
        QQmlComponent pageComponent(&engine, QUrl::fromLocalFile(QStringLiteral(HISTORY_QML)));
        QScopedPointer<QObject> page(pageComponent.createWithInitialProperties({
            {QStringLiteral("notificationModel"), QVariant::fromValue(&model)},
            {QStringLiteral("clearHistory"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("resolveApplicationIcon"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){return '';})")))},
            {QStringLiteral("launchApplication"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("demoNotificationVisible"), false}, {QStringLiteral("maximumHeight"), 900}
        }));
        QVERIFY2(page, qPrintable(pageComponent.errorString()));
        auto *item = qobject_cast<QQuickItem *>(page.data());
        QVERIFY(item);
        QQuickWindow window;
        window.resize(430, 900);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(430, 900));
        window.show();
        const qreal initialImplicitHeight = item->implicitHeight();
        model.append(true, false, true);
        model.append(false, false, true);
        QTRY_VERIFY(item->implicitHeight() > initialImplicitHeight);
        QTRY_VERIFY(item->implicitHeight() <= 900);
        item->setHeight(item->implicitHeight());
        QTest::qWait(50);
        for (int index : {1, 2}) {
            QQuickItem *actionRow = nullptr;
            QTRY_VERIFY((actionRow = findItem(item, QStringLiteral("notificationRow%1").arg(index))));
            auto *actionCard = actionRow->findChild<QQuickItem *>("notificationCard");
            auto *actionFlow = actionRow->findChild<QQuickItem *>("notificationActionFlow");
            auto *firstAction = findItem(actionRow, QStringLiteral("notificationAction-accept"));
            auto *secondAction = findItem(actionRow, QStringLiteral("notificationAction-reject"));
            QVERIFY(actionCard && actionFlow && firstAction && secondAction);
            // A card is told apart by its fill and the space around it, with
            // no outline.
            QCOMPARE(actionCard->property("visualOutlineWidth").toReal(), 0.0);
            QTRY_VERIFY(actionFlow->height() >= 44);
            const QRectF cardRect = actionCard->mapRectToItem(item, actionCard->boundingRect());
            QVERIFY2(cardRect.top() >= 0 && cardRect.bottom() <= item->height() + 0.5,
                "Recent notification cards must fit inside the content-sized popup page");
            QVERIFY2(actionRow->height() >= actionCard->height() + 8,
                "Each recent notification row must reserve a distinct gap after its outlined card");
            for (auto *action : {firstAction, secondAction}) {
                const QRectF actionRect = action->mapRectToItem(actionCard, action->boundingRect());
                QVERIFY2(actionRect.top() >= 0 && actionRect.bottom() <= actionCard->height() + 0.5,
                    "Repeated notification actions must remain inside their card");
            }
        }
        item->setParentItem(nullptr);
    }

    // Every row on screen, top to bottom, must start where the one above it
    // ends, show the notification the model holds at its place, and keep its
    // card, Show more and Dismiss inside itself. Returns what is wrong, or
    // nothing.
    static QString layoutProblem(QQuickItem *page, const HistoryFixture &model) {
        QList<QQuickItem *> rows;
        const std::function<void(QQuickItem *)> collect = [&](QQuickItem *at) {
            const QString name = at->objectName();
            if (name.startsWith(QStringLiteral("notificationRow")) && at->isVisible() && at->height() > 0)
                rows.append(at);
            for (auto *child : at->childItems()) collect(child);
        };
        collect(page);
        if (rows.size() != model.rows.size())
            return QStringLiteral("%1 rows drawn for %2 in the history").arg(rows.size()).arg(model.rows.size());
        std::sort(rows.begin(), rows.end(), [](QQuickItem *a, QQuickItem *b) {
            return a->mapToScene(QPointF()).y() < b->mapToScene(QPointF()).y();
        });
        qreal previousBottom = -1e9;
        for (int position = 0; position < rows.size(); ++position) {
            QQuickItem *row = rows[position];
            const QRectF rowRect = row->mapRectToScene(row->boundingRect());
            if (rowRect.top() < previousBottom - 0.5)
                return QStringLiteral("%1 starts at %2, inside the row above, which ends at %3")
                    .arg(row->objectName()).arg(rowRect.top()).arg(previousBottom);
            previousBottom = rowRect.bottom();
            const int index = row->property("index").toInt();
            if (index != position)
                return QStringLiteral("%1 is drawn in place %2").arg(row->objectName()).arg(position);
            const QVariantMap &held = model.rows[index];
            if (row->property("summary").toString() != held.value(QStringLiteral("summary")).toString()
                    || row->property("isGroup").toBool() != held.value(QStringLiteral("isGroup")).toBool())
                return QStringLiteral("%1 shows another notification than the history holds there").arg(row->objectName());
            auto *card = findItem(row, QStringLiteral("notificationCard"));
            if (!card || !card->isVisible()) continue;
            const QRectF cardRect = card->mapRectToScene(card->boundingRect());
            if (cardRect.bottom() > rowRect.bottom() + 0.5)
                return QStringLiteral("%1's card runs past its row").arg(row->objectName());
            for (const char *part : {"notificationReadMore", "notificationDismiss"}) {
                auto *control = findItem(row, QString::fromLatin1(part));
                if (!control || !control->isVisible()) continue;
                const QRectF controlRect = control->mapRectToScene(control->boundingRect());
                if (controlRect.top() < cardRect.top() - 0.5 || controlRect.bottom() > cardRect.bottom() + 0.5)
                    return QStringLiteral("%1's %2 is outside its card").arg(row->objectName(), QString::fromLatin1(part));
            }
            auto *earlier = findItem(row, QStringLiteral("notificationEarlierLabel"));
            if (earlier && earlier->isVisible()
                    && earlier->mapRectToScene(earlier->boundingRect()).bottom() > cardRect.top() + 0.5)
                return QStringLiteral("%1's Earlier heading runs into its card").arg(row->objectName());
        }
        return {};
    }

    // The history goes through everything that has put cards over one
    // another before: a group opening and closing as one re-sort, Show more,
    // "Earlier" moving, new arrivals, a dismiss, the bell closing and opening,
    // a reset and Clear. After each, no row may reach into another.
    void churnKeepsRowsApart() {
        QQmlEngine engine;
        engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
        using Fixture = HistoryFixture;
        const auto espn = [](const QString &summary, int words) {
            return Fixture::entry(QStringLiteral("ESPN"), summary, words, true);
        };
        const QVariantMap espnGroup = Fixture::entry(QStringLiteral("ESPN"), QStringLiteral("ESPN"), 0, false, true);
        HistoryFixture model;
        model.reset({espnGroup, espn(QStringLiteral("Adam Schefter"), 40),
            Fixture::entry(QStringLiteral("KWin"), QStringLiteral("Graphics card reset"), 30),
            Fixture::entry(QStringLiteral("Target"), QStringLiteral("$5 gift card"), 8),
            Fixture::entry(QStringLiteral("NBA"), QStringLiteral("Wild night"), 45)});
        QQmlComponent pageComponent(&engine, QUrl::fromLocalFile(QStringLiteral(HISTORY_QML)));
        QScopedPointer<QObject> page(pageComponent.createWithInitialProperties({
            {QStringLiteral("notificationModel"), QVariant::fromValue(&model)},
            {QStringLiteral("clearHistory"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("resolveApplicationIcon"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){return '';})")))},
            {QStringLiteral("launchApplication"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("demoNotificationVisible"), false}, {QStringLiteral("maximumHeight"), 3000}
        }));
        QVERIFY2(page, qPrintable(pageComponent.errorString()));
        auto *item = qobject_cast<QQuickItem *>(page.data());
        QVERIFY(item);
        // ESPN's notifications came in during Do Not Disturb, so "Earlier"
        // stands over the first card from another app.
        item->setProperty("cameDuringFocus", QVariant::fromValue(engine.evaluate(QStringLiteral(
            "(function(model, index){ return model.data(index, %1) === 'ESPN'; })")
            .arg(int(HistoryFixture::Roles::ApplicationNameRole)))));
        QQuickWindow window;
        window.resize(430, 3000);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(430, 3000));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        const auto settle = [&](const char *after) {
            QTest::qWait(250);
            const QString problem = layoutProblem(item, model);
            return problem.isEmpty() ? QString() : QStringLiteral("after %1: %2").arg(QLatin1String(after), problem);
        };
        const auto clickShowMore = [&](int row) {
            auto *rowItem = findItem(item, QStringLiteral("notificationRow%1").arg(row));
            auto *more = rowItem ? findItem(rowItem, QStringLiteral("notificationReadMore")) : nullptr;
            if (more && more->isVisible()) QMetaObject::invokeMethod(more, "clicked");
        };
        QString problem;
#define STEP(name, ...) do { __VA_ARGS__; problem = settle(name); QVERIFY2(problem.isEmpty(), qPrintable(problem)); } while (false)

        STEP("the first layout");
        STEP("Do Not Disturb ending",
            item->setProperty("focusArrivals", QVariant::fromValue(engine.evaluate(
                QStringLiteral("({count: 3, from: new Date(), to: new Date()})")))));
        STEP("Show more on the KWin card", clickShowMore(2));
        QList<QVariantMap> opened = model.rows;
        opened[0][QStringLiteral("isGroupExpanded")] = true;
        opened.insert(2, espn(QStringLiteral("Kyler Murray"), 50));
        opened.insert(3, espn(QStringLiteral("Bills"), 5));
        STEP("ESPN opening as one re-sort", model.relayout(opened));
        STEP("Show more on an ESPN card", clickShowMore(1));
        QList<QVariantMap> closed = model.rows;
        closed[0][QStringLiteral("isGroupExpanded")] = false;
        closed.removeAt(3);
        closed.removeAt(2);
        STEP("ESPN closing as one re-sort", model.relayout(closed));
        STEP("a long arrival at the top",
            model.insertAt(0, Fixture::entry(QStringLiteral("Mail"), QStringLiteral("Long letter"), 60)));
        QList<QVariantMap> resorted = model.rows;
        resorted.move(1, 0);
        resorted.move(2, 1);
        resorted.insert(1, espn(QStringLiteral("Breaking"), 35));
        STEP("ESPN moving to the top with a new card", model.relayout(resorted));
        STEP("a card's text growing", model.change(3, QStringLiteral("body"), QStringLiteral("grown ").repeated(80)));
        STEP("a card's text shrinking", model.change(3, QStringLiteral("body"), QStringLiteral("short")));
        STEP("a dismiss", model.removeAt(2));
        STEP("the bell closing and opening", item->setVisible(false); QTest::qWait(50); item->setVisible(true));
        STEP("Do Not Disturb's arrivals being read",
            item->setProperty("focusArrivals", QVariant()));
        STEP("a reset", model.reset({espnGroup, espn(QStringLiteral("Adam Schefter"), 40),
            Fixture::entry(QStringLiteral("KWin"), QStringLiteral("Graphics card reset"), 30)}));
        STEP("Clear", model.reset({}));
        STEP("arrivals after Clear",
            model.insertAt(0, Fixture::entry(QStringLiteral("Target"), QStringLiteral("$5 gift card"), 8));
            model.insertAt(0, Fixture::entry(QStringLiteral("NBA"), QStringLiteral("Wild night"), 45)));
#undef STEP
        item->setParentItem(nullptr);
    }

    // The same check through long runs of changes in random order, each run
    // the same every time: arrivals, dismissals, re-sorts that move, add and
    // drop rows at once, text growing and shrinking, Show more, "Earlier"
    // coming and going, the bell closing, and resets. The history is checked
    // after most changes, and some changes land before the last has settled.
    void churnInRandomOrder_data() {
        QTest::addColumn<quint32>("seed");
        for (quint32 seed : {1u, 2u, 3u})
            QTest::newRow(qPrintable(QStringLiteral("seed %1").arg(seed))) << seed;
    }
    void churnInRandomOrder() {
        QFETCH(quint32, seed);
        quint32 state = seed;
        // A small generator of its own, so a run is the same on every Qt.
        const auto next = [&state](int below) {
            state = (state * 1103515245u + 12345u) & 0x7fffffffu;
            return int((state >> 8) % quint32(below));
        };
        static const QStringList applications{QStringLiteral("ESPN"), QStringLiteral("KWin"),
            QStringLiteral("Target"), QStringLiteral("NBA")};
        static const int words[]{0, 3, 25, 60};
        int serial = 0;
        const auto make = [&]() {
            ++serial;
            const QString application = applications[next(4)];
            const int length = words[next(4)];
            return HistoryFixture::entry(application, QStringLiteral("Alert %1").arg(serial), length, false, next(5) == 0);
        };

        QQmlEngine engine;
        engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
        HistoryFixture model;
        model.closeRemoves = true;
        QList<QVariantMap> first;
        for (int i = 0; i < 5; ++i) first.append(make());
        model.reset(first);
        QQmlComponent pageComponent(&engine, QUrl::fromLocalFile(QStringLiteral(HISTORY_QML)));
        QScopedPointer<QObject> page(pageComponent.createWithInitialProperties({
            {QStringLiteral("notificationModel"), QVariant::fromValue(&model)},
            {QStringLiteral("clearHistory"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("resolveApplicationIcon"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){return '';})")))},
            {QStringLiteral("launchApplication"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("demoNotificationVisible"), false}, {QStringLiteral("maximumHeight"), 3000}
        }));
        QVERIFY2(page, qPrintable(pageComponent.errorString()));
        auto *item = qobject_cast<QQuickItem *>(page.data());
        QVERIFY(item);
        item->setProperty("cameDuringFocus", QVariant::fromValue(engine.evaluate(QStringLiteral(
            "(function(model, index){ return model.data(index, %1) === 'ESPN'; })")
            .arg(int(HistoryFixture::Roles::ApplicationNameRole)))));
        const QJSValue focus = engine.evaluate(QStringLiteral("({count: 3, from: new Date(), to: new Date()})"));
        QQuickWindow window;
        window.resize(430, 3000);
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(430, 3000));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto press = [&](int row, const QString &control) {
            auto *rowItem = findItem(item, QStringLiteral("notificationRow%1").arg(row));
            auto *target = rowItem ? findItem(rowItem, control) : nullptr;
            if (target && target->isVisible()) QMetaObject::invokeMethod(target, "clicked");
        };

        bool focused = false;
        for (int step = 0; step < 40; ++step) {
            int change = next(9);
            const int rows = model.rows.size();
            if (rows < 3) change = 0;
            if (rows > 12) change = 1;
            switch (change) {
            case 0: { const int at = next(rows + 1); model.insertAt(at, make()); break; }
            case 1: model.removeAt(next(rows)); break;
            case 2: {
                QList<QVariantMap> sorted = model.rows;
                if (next(2)) sorted.removeAt(next(sorted.size()));
                for (int added = 1 + next(2); added > 0; --added) {
                    const int at = next(sorted.size() + 1);
                    sorted.insert(at, make());
                }
                const int a = next(sorted.size());
                const int b = next(sorted.size());
                sorted.swapItemsAt(a, b);
                model.relayout(sorted);
                break;
            }
            case 3: {
                const int row = next(rows);
                model.change(row, QStringLiteral("body"),
                    QStringLiteral("grown ").repeated(words[next(4)]).trimmed());
                break;
            }
            case 4: press(next(rows), QStringLiteral("notificationReadMore")); break;
            case 5:
                focused = !focused;
                item->setProperty("focusArrivals", focused ? QVariant::fromValue(focus) : QVariant());
                break;
            case 6: item->setVisible(false); QTest::qWait(30); item->setVisible(true); break;
            case 7: press(next(rows), QStringLiteral("notificationDismiss")); break;
            case 8:
                if (next(4) == 0) {
                    QList<QVariantMap> fresh;
                    for (int count = 3 + next(5); count > 0; --count) fresh.append(make());
                    model.reset(fresh);
                }
                break;
            }
            if (next(10) >= 6) continue;
            QTest::qWait(250);
            const QString problem = layoutProblem(item, model);
            QVERIFY2(problem.isEmpty(), qPrintable(QStringLiteral("step %1, change %2: %3").arg(step).arg(change).arg(problem)));
        }
        item->setParentItem(nullptr);
    }

};
QTEST_MAIN(NotificationInteractionTest)
#include "NotificationInteractionTest.moc"
