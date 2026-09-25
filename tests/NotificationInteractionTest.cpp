// SPDX-License-Identifier: LGPL-2.0-or-later
#include <QApplication>
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
            {Roles::ActionNamesRole, "actionNames"}, {Roles::ActionLabelsRole, "actionLabels"}};
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
    Q_INVOKABLE void close(const QModelIndex &idx) { closedRow = idx.row(); ++closeCalls; }
    bool setData(const QModelIndex &idx, const QVariant &value, int role) override {
        expandedRow = idx.row(); expandedRole = role; expanded = value.toBool();
        rows[idx.row()][QStringLiteral("isGroupExpanded")] = value;
        Q_EMIT dataChanged(idx, idx, {Roles::IsGroupExpandedRole});
        return true;
    }
    void append(bool grouped, bool group = false, bool actionable = false) {
        const int row = rows.size();
        beginInsertRows({}, row, row);
        rows.append({{QStringLiteral("isGroup"), group}, {QStringLiteral("isInGroup"), grouped},
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
                : QStringList{}}});
        endInsertRows();
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
        QCOMPARE(accept->property("visualOutline").value<QColor>(), QColor(QStringLiteral("#F8F8FF")));
        QCOMPARE(accept->property("visualFill").value<QColor>().alpha(), 0);
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

};
QTEST_MAIN(NotificationInteractionTest)
#include "NotificationInteractionTest.moc"
