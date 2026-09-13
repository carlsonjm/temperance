// SPDX-License-Identifier: LGPL-2.0-or-later
#include <QApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <KLocalizedContext>

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
        QQmlComponent modelComponent(&engine);
        modelComponent.setData(R"(
            import QtQuick
            ListModel {
                property int invokedRow: -1
                function index(row, column) { return row; }
                function invokeDefaultAction(row) { invokedRow = row; }
                function close(row) {}
            })", QUrl());
        QScopedPointer<QObject> model(modelComponent.create());
        QVERIFY2(model, qPrintable(modelComponent.errorString()));
        engine.globalObject().setProperty(QStringLiteral("fixtureModel"), engine.newQObject(model.data()));
        engine.globalObject().setProperty(QStringLiteral("grouped"), grouped);
        const auto append = engine.evaluate(QStringLiteral(R"(
            fixtureModel.append({isGroup:false, isInGroup:grouped,
                isGroupExpanded:true, groupChildrenCount:0, summary:'Test alert',
                body:'<html><b>Long formatted body</b><br>' + 'message '.repeat(150) + '</html>',
                applicationName:'Fixture', applicationIconName:'', desktopEntry:'fixture',
                hasDefaultAction:true});
        )"));
        QVERIFY2(!append.isError(), qPrintable(append.toString()));
        QQmlComponent pageComponent(&engine, QUrl::fromLocalFile(QStringLiteral(HISTORY_QML)));
        QScopedPointer<QObject> page(pageComponent.createWithInitialProperties({
            {QStringLiteral("notificationModel"), QVariant::fromValue(model.data())},
            {QStringLiteral("clearHistory"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){})")))},
            {QStringLiteral("resolveApplicationIcon"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(){return '';})")))},
            {QStringLiteral("launchApplication"), QVariant::fromValue(engine.evaluate(QStringLiteral("(function(id){fixtureModel.invokedRow=99;})")))},
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
        QCOMPARE(model->property("invokedRow").toInt(), -1);
        QTRY_VERIFY(!body->property("truncated").toBool());
        row->setProperty("detailsExpanded", false);
        QTest::qWait(50);
        click(summary);
        QTRY_COMPARE(model->property("invokedRow").toInt(), 0);
        row->setProperty("hasDefaultAction", false);
        click(summary);
        QTRY_COMPARE(model->property("invokedRow").toInt(), 99);
        item->setParentItem(nullptr);
    }
};
QTEST_MAIN(NotificationInteractionTest)
#include "NotificationInteractionTest.moc"
