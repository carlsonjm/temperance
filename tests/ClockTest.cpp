// SPDX-License-Identifier: LGPL-2.0-or-later
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJSValue>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QThread>
#include <QTimeZone>
#include <cmath>
#include <memory>
#include <utility>

// The Status Bar gives the clock 34 px between its margins.
static constexpr qreal availableHeight = 34;

static int failures = 0;

static void check(bool condition, const QString &message)
{
    if (condition) return;
    ++failures;
    qWarning().noquote() << "FAIL:" << message;
}

static QQuickItem *findNamed(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (QQuickItem *child : root->childItems())
        if (QQuickItem *found = findNamed(child, name)) return found;
    return nullptr;
}

static QList<QQuickItem *> tokens(QQuickItem *line)
{
    QList<QQuickItem *> result;
    for (QQuickItem *child : line->childItems())
        if (child->objectName().startsWith(QStringLiteral("temperance-clock-")))
            result.append(child);
    return result;
}

static bool fastAnimation = false;

static void settle(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(2);
    }
}

static qreal lineWidth(QQuickItem *line)
{
    qreal width = 0;
    for (QQuickItem *token : tokens(line)) width += token->width();
    return width;
}

// Distance from each digit to the right edge of its line. Right alignment makes
// this the digit's place on screen.
static QList<qreal> digitOffsets(QQuickItem *line)
{
    const qreal total = lineWidth(line);
    QList<qreal> result;
    qreal x = 0;
    for (QQuickItem *token : tokens(line)) {
        if (token->objectName() == QStringLiteral("temperance-clock-digit"))
            result.append(total - x);
        x += token->width();
    }
    return result;
}

static void exercise(QQmlEngine &engine, QQmlComponent &component,
                     const QString &family, const QString &localeName)
{
    const QString label = QStringLiteral("%1 / %2").arg(family, localeName);
    std::unique_ptr<QObject> object(component.create());
    auto *clock = qobject_cast<QQuickItem *>(object.get());
    if (!clock) {
        check(false, label + QStringLiteral(": clock did not load: ") + component.errorString());
        return;
    }
    clock->setProperty("fontFamily", family);
    clock->setProperty("clockLocale", QVariant::fromValue(
        engine.evaluate(QStringLiteral("Qt.locale('%1')").arg(localeName))));
    QCoreApplication::processEvents();

    QQuickItem *time = findNamed(clock, QStringLiteral("temperance-clock-time"));
    QQuickItem *date = findNamed(clock, QStringLiteral("temperance-clock-date"));
    check(time && date, label + QStringLiteral(": both lines are present"));
    if (!time || !date) return;
    check(clock->property("primaryFont").value<QFont>().family() == family,
          label + QStringLiteral(": the chosen family is the one drawn"));
    check(clock->implicitHeight() <= availableHeight,
          label + QStringLiteral(": %1 px fits the bar's %2 px")
                      .arg(clock->implicitHeight()).arg(availableHeight));

    const QTimeZone zone = QTimeZone::systemTimeZone();
    const qreal reserved = clock->implicitWidth();
    check(reserved > 0, label + QStringLiteral(": the clock reserves a width"));

    // Every minute of a day: the reserved width never changes, the time always
    // fits it, and the two minute digits never move.
    QList<qreal> minuteOffsets;
    for (int minute = 0; minute < 24 * 60; ++minute) {
        clock->setProperty("dateTime", QDateTime(QDate(2026, 9, 25), QTime(minute / 60, minute % 60), zone));
        QCoreApplication::processEvents();
        if (clock->implicitWidth() != reserved) {
            check(false, label + QStringLiteral(": width changed at %1 to %2 from %3")
                                     .arg(clock->property("timeText").toString())
                                     .arg(clock->implicitWidth()).arg(reserved));
            return;
        }
        if (lineWidth(time) > reserved + 0.5) {
            check(false, label + QStringLiteral(": %1 overflows its room")
                                     .arg(clock->property("timeText").toString()));
            return;
        }
        const QList<qreal> offsets = digitOffsets(time);
        if (offsets.size() < 3) {
            check(false, label + QStringLiteral(": %1 has no digit cells")
                                     .arg(clock->property("timeText").toString()));
            return;
        }
        const QList<qreal> minutes = offsets.mid(offsets.size() - 2);
        if (minuteOffsets.isEmpty()) minuteOffsets = minutes;
        if (minutes != minuteOffsets) {
            check(false, label + QStringLiteral(": minute digits moved at %1")
                                     .arg(clock->property("timeText").toString()));
            return;
        }
    }

    // Every day of two years: the width holds and the date always fits.
    for (QDate day(2026, 1, 1); day < QDate(2028, 1, 1); day = day.addDays(1)) {
        clock->setProperty("dateTime", QDateTime(day, QTime(12, 0), zone));
        QCoreApplication::processEvents();
        if (clock->implicitWidth() != reserved || lineWidth(date) > reserved + 0.5) {
            check(false, label + QStringLiteral(": %1 changed the width or overflows")
                                     .arg(clock->property("dateText").toString()));
            return;
        }
    }
    qInfo().noquote() << "PASS:" << label << "reserves" << reserved << "x"
                      << clock->implicitHeight();
}

static std::unique_ptr<QObject> createClock(QQmlEngine &engine, QQmlComponent &component,
                                            const QString &family)
{
    std::unique_ptr<QObject> object(component.create());
    if (!object) return object;
    object->setProperty("fontFamily", family);
    object->setProperty("clockLocale", QVariant::fromValue(engine.evaluate(QStringLiteral("Qt.locale('en_US')"))));
    QCoreApplication::processEvents();
    return object;
}

// Either line can be shown alone; a lone line takes the primary style and the
// height of one line.
static void exerciseLines(QQmlEngine &engine, QQmlComponent &component, const QString &family)
{
    const QString label = family + QStringLiteral(" / one line");
    std::unique_ptr<QObject> object = createClock(engine, component, family);
    auto *clock = qobject_cast<QQuickItem *>(object.get());
    if (!clock) { check(false, label + QStringLiteral(": clock did not load")); return; }
    QQuickItem *time = findNamed(clock, QStringLiteral("temperance-clock-time"));
    QQuickItem *date = findNamed(clock, QStringLiteral("temperance-clock-date"));
    const qreal twoLineHeight = clock->implicitHeight();

    clock->setProperty("showDate", false);
    QCoreApplication::processEvents();
    const qreal oneLineHeight = clock->implicitHeight();
    check(time->isVisible() && !date->isVisible(), label + QStringLiteral(": time alone shows only the time"));
    check(oneLineHeight > 0 && oneLineHeight < twoLineHeight,
          label + QStringLiteral(": time alone is one line high"));
    check(clock->implicitWidth() == std::ceil(clock->property("reservedTimeWidth").toReal()),
          label + QStringLiteral(": time alone reserves only the time's width"));

    clock->setProperty("showTime", false);
    clock->setProperty("showDate", true);
    QCoreApplication::processEvents();
    QQuickItem *dateGlyph = findNamed(date, QStringLiteral("temperance-clock-glyph"));
    check(date->isVisible() && !time->isVisible(), label + QStringLiteral(": date alone shows only the date"));
    check(dateGlyph && dateGlyph->property("font").value<QFont>().pixelSize()
              == clock->property("primaryFont").value<QFont>().pixelSize(),
          label + QStringLiteral(": date alone takes the primary style"));
    check(clock->implicitHeight() == oneLineHeight, label + QStringLiteral(": date alone is one line high"));
    qInfo().noquote() << "PASS:" << label;
}

// The day period is a lowercase label, lighter than the time it follows.
static void exercisePeriod(QQmlEngine &engine, QQmlComponent &component, const QString &family)
{
    const QString label = family + QStringLiteral(" / day period");
    std::unique_ptr<QObject> object = createClock(engine, component, family);
    auto *clock = qobject_cast<QQuickItem *>(object.get());
    if (!clock) { check(false, label + QStringLiteral(": clock did not load")); return; }
    QQuickItem *time = findNamed(clock, QStringLiteral("temperance-clock-time"));
    const QTimeZone zone = QTimeZone::systemTimeZone();
    for (const auto &[hour, period] : {std::pair{7, QStringLiteral("a.m.")}, std::pair{19, QStringLiteral("p.m.")}}) {
        clock->setProperty("dateTime", QDateTime(QDate(2026, 9, 25), QTime(hour, 49), zone));
        QCoreApplication::processEvents();
        const QString text = clock->property("timeText").toString();
        check(text.endsWith(period) && !text.contains(QStringLiteral("AM")) && !text.contains(QStringLiteral("PM")),
              label + QStringLiteral(": %1 reads %2").arg(text, period));
    }
    settle(500);
    const QList<QQuickItem *> parts = tokens(time);
    QQuickItem *suffix = parts.isEmpty() ? nullptr : findNamed(parts.last(), QStringLiteral("temperance-clock-glyph"));
    QQuickItem *digit = parts.isEmpty() ? nullptr : findNamed(parts.first(), QStringLiteral("temperance-clock-glyph"));
    check(suffix && suffix->property("text").toString().trimmed() == QStringLiteral("p.m."),
          label + QStringLiteral(": the day period is drawn as its own run"));
    check(suffix && suffix->property("font").value<QFont>().weight() == QFont::Light,
          label + QStringLiteral(": the day period is light"));
    check(suffix && digit
              && suffix->property("font").value<QFont>().pixelSize() < digit->property("font").value<QFont>().pixelSize(),
          label + QStringLiteral(": the day period is smaller than the time"));
    auto baseline = [](QQuickItem *glyph) {
        return glyph->parentItem()->y() + glyph->y() + glyph->property("baselineOffset").toReal();
    };
    check(suffix && digit && std::abs(baseline(suffix) - baseline(digit)) < 0.5,
          label + QStringLiteral(": the day period sits on the time's baseline (%1 against %2)")
                      .arg(suffix ? baseline(suffix) : 0).arg(digit ? baseline(digit) : 0));
    check(digit && digit->property("font").value<QFont>().weight() == QFont::Normal,
          label + QStringLiteral(": the time is regular"));
    qInfo().noquote() << "PASS:" << label;
}

// A new minute deals its changed digit onto the old one and settles; digits
// that did not change never move, and reduced motion only fades. The deal keeps
// its own pace whatever Plasma's animation speed is.
static void exerciseDeal(QQmlEngine &engine, QQmlComponent &component, const QString &family)
{
    const QString label = family + QStringLiteral(" / deal");
    std::unique_ptr<QObject> object = createClock(engine, component, family);
    auto *clock = qobject_cast<QQuickItem *>(object.get());
    if (!clock) { check(false, label + QStringLiteral(": clock did not load")); return; }
    if (fastAnimation)
        check(!clock->property("reducedMotion").toBool(),
              label + QStringLiteral(": a faster animation speed is not reduced motion"));
    clock->setProperty("reducedMotion", false);
    check(clock->property("dealDuration").toInt() >= 160,
          label + QStringLiteral(": the deal lasts %1 ms, long enough to see")
                      .arg(clock->property("dealDuration").toInt()));
    const QTimeZone zone = QTimeZone::systemTimeZone();
    QQuickItem *time = findNamed(clock, QStringLiteral("temperance-clock-time"));
    auto digitAt = [&](int index) -> QQuickItem * {
        int seen = 0;
        for (QQuickItem *token : tokens(time))
            if (token->objectName() == QStringLiteral("temperance-clock-digit") && seen++ == index) return token;
        return nullptr;
    };
    auto glyphOf = [](QQuickItem *token) { return findNamed(token, QStringLiteral("temperance-clock-glyph")); };
    auto outgoingOf = [](QQuickItem *token) { return findNamed(token, QStringLiteral("temperance-clock-outgoing")); };

    clock->setProperty("dateTime", QDateTime(QDate(2026, 9, 25), QTime(7, 48), zone));
    settle(500);
    clock->setProperty("dateTime", QDateTime(QDate(2026, 9, 25), QTime(7, 49), zone));
    settle(40);
    QQuickItem *changed = digitAt(2);
    QQuickItem *still = digitAt(0);
    if (!changed || !still) { check(false, label + QStringLiteral(": digits not found")); return; }
    check(glyphOf(changed)->property("text").toString() == QStringLiteral("9")
              && outgoingOf(changed)->property("text").toString() == QStringLiteral("8"),
          label + QStringLiteral(": the new digit is dealt onto the old one"));
    check(glyphOf(changed)->y() < 0 && glyphOf(changed)->opacity() < 1 && outgoingOf(changed)->opacity() > 0
              && glyphOf(changed)->scale() > 1 && glyphOf(changed)->rotation() != 0,
          label + QStringLiteral(": the new digit is in the air, nearer and turned"));
    check(glyphOf(still)->y() == 0 && glyphOf(still)->opacity() == 1 && outgoingOf(still)->opacity() == 0,
          label + QStringLiteral(": an unchanged digit stays still"));
    settle(600);
    check(glyphOf(changed)->y() == 0 && glyphOf(changed)->opacity() == 1 && outgoingOf(changed)->opacity() == 0
              && glyphOf(changed)->scale() == 1 && glyphOf(changed)->rotation() == 0,
          label + QStringLiteral(": the deal settles flat with nothing left behind"));

    clock->setProperty("reducedMotion", true);
    clock->setProperty("dateTime", QDateTime(QDate(2026, 9, 25), QTime(7, 50), zone));
    settle(40);
    QQuickItem *faded = digitAt(1);
    check(faded && glyphOf(faded)->y() == 0 && glyphOf(faded)->opacity() < 1
              && glyphOf(faded)->scale() == 1 && glyphOf(faded)->rotation() == 0,
          label + QStringLiteral(": under reduced motion the digit fades without travel"));
    settle(600);
    check(faded && glyphOf(faded)->opacity() == 1 && outgoingOf(faded)->opacity() == 0,
          label + QStringLiteral(": the fade settles"));
    qInfo().noquote() << "PASS:" << label;
}

// The lock's block: a bold time over the long date, the date exactly as wide as
// the time on every day of the year, its weekday bold and the rest not, and
// the bar's two lines out of the way.
static void exerciseBlock(QQmlEngine &engine, QQmlComponent &component, const QString &family)
{
    const QString label = family + QStringLiteral(" / block");
    std::unique_ptr<QObject> object = createClock(engine, component, family);
    auto *clock = qobject_cast<QQuickItem *>(object.get());
    if (!clock) { check(false, label + QStringLiteral(": clock did not load")); return; }
    // Its lines are rows, which lay themselves out only in a window, and the
    // block's width follows theirs.
    QQuickWindow window;
    window.resize(800, 300);
    clock->setParentItem(window.contentItem());
    window.show();
    clock->setProperty("presentation", QStringLiteral("block"));
    clock->setProperty("timePixelSize", 76);
    const QTimeZone zone = QTimeZone::systemTimeZone();
    clock->setProperty("dateTime", QDateTime(QDate(2026, 9, 25), QTime(13, 23), zone));
    settle(500);

    QQuickItem *time = findNamed(clock, QStringLiteral("temperance-clock-block-time"));
    QQuickItem *date = findNamed(clock, QStringLiteral("temperance-clock-block-date"));
    QQuickItem *weekday = findNamed(clock, QStringLiteral("temperance-clock-block-weekday"));
    QQuickItem *barTime = findNamed(clock, QStringLiteral("temperance-clock-time"));
    if (!time || !date || !weekday || !barTime) { check(false, label + QStringLiteral(": a line is missing")); return; }

    check(time->isVisible() && date->isVisible() && !barTime->isVisible(),
          label + QStringLiteral(": the block shows its own lines and not the bar's"));
    check(weekday->property("text").toString() == QStringLiteral("Friday"),
          label + QStringLiteral(": the weekday is written out (%1)").arg(weekday->property("text").toString()));
    QQuickItem *digit = findNamed(time, QStringLiteral("temperance-clock-glyph"));
    check(digit && digit->property("font").value<QFont>().weight() == QFont::Bold
              && digit->property("font").value<QFont>().pixelSize() == 76,
          label + QStringLiteral(": the time is bold at its own size"));
    check(weekday->property("font").value<QFont>().weight() == QFont::Bold,
          label + QStringLiteral(": the weekday is bold"));
    const QList<QQuickItem *> dateParts = date->childItems();
    check(dateParts.size() == 2 && dateParts.last()->property("font").value<QFont>().weight() < QFont::DemiBold,
          label + QStringLiteral(": the rest of the date is not bold"));
    check(clock->implicitWidth() == std::ceil(time->width()),
          label + QStringLiteral(": the block is as wide as its time"));

    for (QDate day(2026, 1, 1); day < QDate(2027, 1, 1); day = day.addDays(1)) {
        clock->setProperty("dateTime", QDateTime(day, QTime(13, 23), zone));
        settle(20);
        if (std::abs(date->width() - time->width()) > 0.5) {
            check(false, label + QStringLiteral(": %1 is %2 wide against the time's %3")
                                     .arg(day.toString(Qt::ISODate)).arg(date->width()).arg(time->width()));
            return;
        }
    }
    qInfo().noquote() << "PASS:" << label << "at" << clock->implicitWidth() << "x" << clock->implicitHeight();
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    fastAnimation = app.arguments().contains(QStringLiteral("--fast-animation"));
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(STATUS_CLOCK_QML)));
    if (component.isError()) {
        qWarning().noquote() << component.errorString();
        return 1;
    }

    // Run under a kdeglobals whose animation speed is four times the default,
    // which shortens Kirigami's durations to a quarter.
    if (fastAnimation) {
        QQmlComponent units(&engine);
        units.setData("import QtQml\nimport org.kde.kirigami as Kirigami\n"
                      "QtObject { readonly property int longDuration: Kirigami.Units.longDuration }",
                      QUrl());
        std::unique_ptr<QObject> probe(units.create());
        const int longDuration = probe ? probe->property("longDuration").toInt() : -1;
        if (longDuration < 0 || longDuration >= 100) {
            qInfo().noquote() << "SKIP: Kirigami's durations did not follow the fast animation speed:"
                              << longDuration;
            return 0;
        }
        const QStringList families = QFontDatabase::families();
        const QString family = families.contains(QStringLiteral("Manrope")) ? QStringLiteral("Manrope")
                                                                             : QStringLiteral("DejaVu Sans");
        exerciseDeal(engine, component, family);
        return failures == 0 ? 0 : 1;
    }

    // Neulis Alt has proportional digits and no tabular figures, which is the
    // case the cells exist for, and Manrope's default digits are proportional
    // too. Each family is exercised where it is installed.
    const QStringList families{QStringLiteral("Neulis Alt"), QStringLiteral("Manrope"),
                               QStringLiteral("DejaVu Sans"), QStringLiteral("Noto Sans")};
    const QStringList available = QFontDatabase::families();
    int exercised = 0;
    for (const QString &family : families) {
        if (!available.contains(family)) continue;
        ++exercised;
        for (const QString &locale : {QStringLiteral("en_US"), QStringLiteral("en_GB"), QStringLiteral("de_DE")})
            exercise(engine, component, family, locale);
        exerciseLines(engine, component, family);
        exercisePeriod(engine, component, family);
        exerciseDeal(engine, component, family);
        exerciseBlock(engine, component, family);
    }
    check(exercised > 0, QStringLiteral("no test font is installed"));
    return failures == 0 ? 0 : 1;
}
