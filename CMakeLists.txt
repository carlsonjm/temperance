cmake_minimum_required(VERSION 3.24)

project(temperance VERSION 1.1.0 LANGUAGES CXX)

set(QT_MIN_VERSION "6.9.0")
set(KF6_MIN_VERSION "6.20.0")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs6)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMQtDeclareLoggingCategory)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core DBus Gui GuiPrivate Quick Widgets)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
    Config CoreAddons IconThemes I18n ItemModels JobWidgets KIO
    Service StatusNotifierItem WindowSystem XmlGui
)
find_package(Plasma REQUIRED)
find_package(PlasmaQuick REQUIRED)
find_package(LibNotificationManager REQUIRED)
find_package(KF6PulseAudioQt REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# This first target is explicitly KDE Wayland-only. Keeping the switch local
# makes the inherited tray code build without depending on workspace internals.
configure_file(config-X11.h.in config-X11.h)

set(libdbusmenu_SRCS
    libdbusmenuqt/dbusmenuimporter.cpp
    libdbusmenuqt/dbusmenushortcut_p.cpp
    libdbusmenuqt/dbusmenutypes_p.cpp
    libdbusmenuqt/utils.cpp
)

ecm_qt_declare_logging_category(libdbusmenu_SRCS
    HEADER libdbusmenuqt/debug.h
    IDENTIFIER DBUSMENUQT
    CATEGORY_NAME studio.warbler.temperance.dbusmenu
    DEFAULT_SEVERITY Info
)

set_source_files_properties(libdbusmenuqt/com.canonical.dbusmenu.xml PROPERTIES
    NO_NAMESPACE true
    INCLUDE "dbusmenutypes_p.h"
    CLASSNAME DBusMenuInterface
)
qt_add_dbus_interface(libdbusmenu_SRCS
    libdbusmenuqt/com.canonical.dbusmenu.xml dbusmenu_interface)

add_library(temperance_dbusmenu STATIC ${libdbusmenu_SRCS})
set_property(TARGET temperance_dbusmenu PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(temperance_dbusmenu PUBLIC
    libdbusmenuqt ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/libdbusmenuqt)
target_link_libraries(temperance_dbusmenu PRIVATE Qt6::DBus Qt6::Widgets)

set(statusarea_SRCS
    src/dbusserviceobserver.cpp
    src/plasmoidregistry.cpp
    src/statusnotifieritemhost.cpp
    src/statusnotifieritemsource.cpp
    src/systemtraysettings.cpp
    src/systemtraytypes.cpp
    src/systemtraymodel.cpp
    src/sortedsystemtraymodel.cpp
)

qt_add_dbus_interface(statusarea_SRCS
    ${KSTATUSNOTIFIERITEM_DBUS_INTERFACES_DIR}/kf6_org.kde.StatusNotifierWatcher.xml
    statusnotifierwatcher_interface)
qt_add_dbus_interface(statusarea_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/dbus/org.freedesktop.DBus.Properties.xml
    dbusproperties)

set(statusnotifieritem_xml
    ${KSTATUSNOTIFIERITEM_DBUS_INTERFACES_DIR}/kf6_org.kde.StatusNotifierItem.xml)
set_source_files_properties(${statusnotifieritem_xml} PROPERTIES
    NO_NAMESPACE false
    INCLUDE "systemtraytypes.h"
    CLASSNAME OrgKdeStatusNotifierItem
)
qt_add_dbus_interface(statusarea_SRCS ${statusnotifieritem_xml} statusnotifieritem_interface)

ecm_qt_declare_logging_category(statusarea_SRCS
    HEADER src/debug.h
    IDENTIFIER SYSTEM_TRAY
    CATEGORY_NAME studio.warbler.temperance
    DEFAULT_SEVERITY Info
)

add_library(temperance_core STATIC ${statusarea_SRCS})
set_property(TARGET temperance_core PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(temperance_core PUBLIC
    src libdbusmenuqt ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/src)
target_link_libraries(temperance_core PUBLIC
    Qt6::DBus Qt6::Gui Qt6::GuiPrivate Qt6::Quick
    KF6::ConfigCore KF6::IconThemes KF6::I18n KF6::ItemModels
    KF6::JobWidgets KF6::KIOGui KF6::Service KF6::WindowSystem KF6::XmlGui
    Plasma::Plasma Plasma::PlasmaQuick temperance_dbusmenu
)

plasma_add_applet(studio.warbler.temperance
    CPP_SOURCES src/systemtray.cpp
    QML_SOURCES
        qml/AbstractItem.qml
        qml/BackgroundAppItem.qml
        qml/BellGlyph.qml
        qml/CompactApplet.qml
        qml/ConfigAppearance.qml
        qml/ConfigGeneral.qml
        qml/ControlCenterPage.qml
        qml/CurrentItemHighLight.qml
        qml/ExpandedRepresentation.qml
        qml/ExpanderArrow.qml
        qml/HiddenItemsView.qml
        qml/ItemLoader.qml
        qml/NotificationHistoryPage.qml
        qml/OrganizedTrayPage.qml
        qml/PlasmoidItem.qml
        qml/PlasmoidPopupsContainer.qml
        qml/PulseAnimation.qml
        qml/StatusNotifierItem.qml
        qml/SystemTrayState.qml
        qml/config.qml
        qml/main.qml
    RESOURCES main.xml
)

install(FILES assets/studio.warbler.temperance.png
    DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/512x512/apps)

target_compile_definitions(studio.warbler.temperance PRIVATE
    TRANSLATION_DOMAIN="temperance")
target_include_directories(studio.warbler.temperance PRIVATE
    src ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/src)
target_link_libraries(studio.warbler.temperance PRIVATE
    temperance_core KF6::CoreAddons KF6::PulseAudioQt PW::LibNotificationManager)

add_test(NAME temperance_source_sanity
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/source_sanity.cmake)

if(BUILD_TESTING)
    find_package(KF6Package REQUIRED)
    add_executable(temperance-panel-load-test tests/PanelLoadTest.cpp)
    target_link_libraries(temperance-panel-load-test PRIVATE
        Qt6::Widgets Qt6::Quick Plasma::Plasma Plasma::PlasmaQuick KF6::Package)
    add_dependencies(temperance-panel-load-test studio.warbler.temperance)
    add_test(NAME temperance_panel_load
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/run-panel-test.sh
            $<TARGET_FILE:temperance-panel-load-test>
            ${CMAKE_CURRENT_BINARY_DIR}/bin)
    set_tests_properties(temperance_panel_load PROPERTIES TIMEOUT 30)
endif()
