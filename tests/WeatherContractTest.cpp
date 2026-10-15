// SPDX-License-Identifier: GPL-2.0-or-later
// The weather icon reads parts of Plasma's Weather widget that are not
// promised to stay (qml/WeatherReading.qml). This loads the installed widget
// and checks that they are still there, so a Plasma release that moves them
// fails here rather than leaving the Status Bar showing "—°". Without the
// widget installed there is nothing to check.
#include <QCoreApplication>
#include <QFile>
#include <QLibrary>
#include <QLibraryInfo>

#include <cstdio>

static int failures = 0;

static void require(bool condition, const char *message)
{
    if (condition) return;
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", message);
}

static QString resource(const QString &name)
{
    QFile file(QStringLiteral(":/qt/qml/plasma/applet/org/kde/plasma/weather/") + name);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString path = QLibraryInfo::path(QLibraryInfo::PluginsPath)
        + QStringLiteral("/plasma/applets/org.kde.plasma.weather.so");
    if (!QFile::exists(path)) {
        std::printf("SKIP: Plasma's Weather widget is not installed at %s\n", qPrintable(path));
        return 77;
    }
    QLibrary library(path);
    if (!library.load()) {
        std::fprintf(stderr, "FAIL: the Weather widget does not load: %s\n", qPrintable(library.errorString()));
        return 1;
    }
    const QString main = resource(QStringLiteral("main.qml"));
    const QString full = resource(QStringLiteral("FullRepresentation.qml"));
    const QString config = resource(QStringLiteral("main.xml"));
    require(!main.isEmpty() && !full.isEmpty() && !config.isEmpty(), "the Weather widget's files were not found");
    // Its saved place, which the online fallback resolves when nothing else
    // is there.
    require(config.contains(QStringLiteral("name=\"placeDisplayName\"")), "the Weather widget no longer saves placeDisplayName");
    // Its live reading and its station, read through its full view.
    require(main.contains(QStringLiteral("fullRepresentation: FullRepresentation")),
            "the Weather widget's full view is no longer FullRepresentation");
    require(full.contains(QStringLiteral("property var lastObservation")), "FullRepresentation no longer has lastObservation");
    require(full.contains(QStringLiteral("property var station")), "FullRepresentation no longer has station");
    require(main.contains(QStringLiteral("toolTipSubText")) && main.contains(QStringLiteral("toolTipMainText")),
            "the Weather widget no longer sets its tooltip");
    if (failures == 0) std::printf("PASS: the Weather widget still offers what the weather icon reads\n");
    return failures == 0 ? 0 : 1;
}
