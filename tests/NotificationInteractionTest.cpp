// SPDX-License-Identifier: LGPL-2.0-or-later
#include <QApplication>
#include <QAbstractListModel>
#include <notifications.h>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <KLocalizedContext>

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
    void append(bool grouped, bool group = false) {
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
            {QStringLiteral("actionNames"), QStringList{}}, {QStringLiteral("actionLabels"), QStringList{}}});
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
        QVERIFY(more && body && summary);
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
        auto *device = touch ? QTest::createTouchDevice() : nullptr;
        const auto click = [&](QQuickItem *target) {
            const auto point = target->mapToScene(QPointF(target->width()/2, target->height()/2)).toPoint();
            if (touch) {
                QTest::touchEvent(&window, device).press(0, point, &window);
                QTest::touchEvent(&window, device).release(0, point, &window);
            } else QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, point);
        };
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

};
QTEST_MAIN(NotificationInteractionTest)
#include "NotificationInteractionTest.moc"
