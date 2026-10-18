# SPDX-FileCopyrightText: 2026 carlsonjm
# SPDX-License-Identifier: ISC

# Structural rules no running test reaches: what is wired to what, what
# stays configured by default, and which files belong together. Sizes, words
# and timing are measured by the QML tests (NotificationInteractionTest,
# PanelLoadTest, ClockTest, CalendarTest), not pinned here, so restyling or
# moving a value into a style object does not fail this.

file(READ "${SOURCE_DIR}/qml/main.qml" main_qml)
file(READ "${SOURCE_DIR}/qml/AlertBanners.qml" banners_qml)
file(READ "${SOURCE_DIR}/qml/ControlCenterPage.qml" control_center_qml)
file(READ "${SOURCE_DIR}/qml/OrganizedTrayPage.qml" organized_tray_qml)
file(READ "${SOURCE_DIR}/qml/ExpandedRepresentation.qml" expanded_qml)
file(READ "${SOURCE_DIR}/qml/StatusNotifierItem.qml" status_notifier_qml)
file(READ "${SOURCE_DIR}/main.xml" config_xml)
file(READ "${SOURCE_DIR}/metadata.json" root_metadata)
file(READ "${SOURCE_DIR}/src/metadata.json" plugin_metadata)
file(READ "${SOURCE_DIR}/src/systemtray.cpp" systemtray_cpp)
file(READ "${SOURCE_DIR}/src/performancecontroller.cpp" performance_cpp)

# A banner window never has zero height, which Wayland rejects; the banner
# host is a window of the live panel, which no automatic test opens.
string(FIND "${banners_qml}" "height: Math.max(1, stackHeight)" safe_banner_height)
string(FIND "${banners_qml}" "priorityNotifications.count > 0 && bannerStack.stackHeight > 0" safe_banner_visibility)
if(safe_banner_height LESS 0 OR safe_banner_visibility LESS 0)
    message(FATAL_ERROR "Banner windows must never expose zero-height Wayland geometry")
endif()
# Its actions keep a touch target of at least 44 px for the same reason.
if(NOT banners_qml MATCHES "component BannerActionButton: Item"
   OR NOT banners_qml MATCHES "Layout.minimumWidth: 44")
    message(FATAL_ERROR "Important-alert actions must keep touch-safe targets")
endif()
if(NOT main_qml MATCHES "id: priorityNotifications"
   OR NOT main_qml MATCHES "minimizePriorityNotification"
   OR NOT main_qml MATCHES "reservedForBanner")
    message(FATAL_ERROR "Important alerts keep their own model, a keep-for-review path, and stay out of the ticker")
endif()

# Clearing the history dismisses every entry, active alerts included.
string(FIND "${main_qml}" "notificationHistory.close(notificationHistory.index(row, 0))" clear_all_position)
if(clear_all_position LESS 0
   OR main_qml MATCHES "notificationHistory.clear\\(NotificationManager.Notifications.ClearExpired\\)")
    message(FATAL_ERROR "Clear history must dismiss active alerts, not only expired notifications")
endif()

# The highlight follows the color scheme unless the person gave it its own.
if(NOT config_xml MATCHES "<entry name=\"accentFromTheme\" type=\"Bool\">[^<]*<label>[^<]*</label>[^<]*<default>true</default>"
   OR NOT main_qml MATCHES "accentFromTheme\n        \\? Kirigami.Theme.highlightColor")
    message(FATAL_ERROR "The highlight must follow the color scheme by default")
endif()

if(NOT main_qml MATCHES "motionEnabled: Kirigami.Units.longDuration > 0")
    message(FATAL_ERROR "Motion must follow the platform animation setting")
endif()

if(NOT main_qml MATCHES "id: hiddenModel\n        dynamicSortFilter: !root.trayHeld"
   OR NOT main_qml MATCHES "id: organizedTrayModel\n        dynamicSortFilter: !root.trayHeld")
    message(FATAL_ERROR "The tray must hold its order while the popup is open")
endif()

if(NOT status_notifier_qml MATCHES "Plasmoid.activate\\(model.DataEngineSource, pos, taskIcon\\)[^}]*systemTrayState.expanded = false")
    message(FATAL_ERROR "Tapping an application's tray entry must close the popup, so the keys' entry types into the window in use")
endif()

# The tray's sections run Apps, Devices, System, by the categories they hold.
string(FIND "${organized_tray_qml}" "\"ApplicationStatus\"" apps_position)
string(FIND "${organized_tray_qml}" "\"Hardware\"" devices_position)
string(FIND "${organized_tray_qml}" "\"SystemServices\"" system_position)
if(apps_position LESS 0 OR devices_position LESS apps_position OR system_position LESS devices_position)
    message(FATAL_ERROR "Tray sections must remain ordered Apps, Devices, System")
endif()

string(FIND "${expanded_qml}" "id: sessionActions" session_actions_position)
if(session_actions_position LESS 0 OR control_center_qml MATCHES "id: sessionActions")
    message(FATAL_ERROR "Session controls belong in the popup header, not content")
endif()
if(NOT expanded_qml MATCHES "org.kde.bluedevilwizard")
    message(FATAL_ERROR "Bluetooth must offer the native pairing wizard")
endif()

# Settings that ship on.
foreach(session_toggle showLock showLogout showRestart showShutdown)
    if(NOT config_xml MATCHES "<entry name=\"${session_toggle}\" type=\"Bool\">[\n\r ]*<label>[^<]+</label>[\n\r ]*<default>true</default>")
        message(FATAL_ERROR "Session control ${session_toggle} must default enabled")
    endif()
endforeach()
if(NOT config_xml MATCHES "<entry name=\"showSwitchUser\" type=\"Bool\">[\n\r ]*<label>[^<]+</label>[\n\r ]*<default>false</default>")
    message(FATAL_ERROR "Switch User must remain opt-in")
endif()
if(NOT config_xml MATCHES "<entry name=\"adaptiveWidth\" type=\"Bool\">[\n\r ]*<label>[^<]+</label>[\n\r ]*<default>true</default>")
    message(FATAL_ERROR "Responsive panel sizing must remain enabled by default")
endif()

# Packaging and identity.
if(NOT root_metadata STREQUAL plugin_metadata)
    message(FATAL_ERROR "Root and native-plugin metadata must remain identical")
endif()
if(NOT root_metadata MATCHES "\"Icon\"[ ]*:[ ]*\"co.goodinput.temperance\""
   OR NOT EXISTS "${SOURCE_DIR}/assets/co.goodinput.temperance.png")
    message(FATAL_ERROR "Widget metadata must reference the Temperance icon, which must exist")
endif()
if(NOT EXISTS "${SOURCE_DIR}/assets/icons/manifest.json"
   OR NOT EXISTS "${SOURCE_DIR}/assets/icons/lucide/LICENSE")
    message(FATAL_ERROR "Pinned Lucide suite action assets are incomplete")
endif()

# The performance helper is run on demand, never on a repeating timer.
if(performance_cpp MATCHES "setInterval\\(" OR performance_cpp MATCHES "QTimer m_")
    message(FATAL_ERROR "The performance helper must not be run on a timer")
endif()

# A build for another Plasma or Qt touches none of their private parts: it
# neither takes the notification service nor builds the tray or session controls.
string(FIND "${systemtray_cpp}" "if (m_needsUpdate) return initNeedingUpdate();" refuse_position)
string(FIND "${systemtray_cpp}" "NotificationManager::Server::self().init()" server_position)
string(FIND "${systemtray_cpp}" "to Plasma until a rebuild\";\n        return;" refuse_ctor_position)
string(FIND "${systemtray_cpp}" "m_session->start();" session_position)
if(refuse_position LESS 0 OR server_position LESS refuse_position
   OR refuse_ctor_position LESS 0 OR session_position LESS refuse_ctor_position
   OR main_qml MATCHES "showNotifications: true"
   OR NOT main_qml MATCHES "showNotifications: !root.needsUpdate")
    message(FATAL_ERROR "A build for another Plasma or Qt must leave notifications and the tray to Plasma")
endif()
