# SPDX-FileCopyrightText: 2026 carlsonjm
# SPDX-License-Identifier: ISC

file(READ "${SOURCE_DIR}/qml/main.qml" main_qml)
string(FIND "${main_qml}" "height: Math.max(1, stackHeight)" safe_banner_height)
string(FIND "${main_qml}" "priorityNotifications.count > 0 && bannerStack.stackHeight > 0" safe_banner_visibility)
if(safe_banner_height LESS 0 OR safe_banner_visibility LESS 0)
    message(FATAL_ERROR "Banner windows must never expose zero-height Wayland geometry")
endif()
if(NOT main_qml MATCHES "implicitHeight: Math.max\\(68, criticalContent.implicitHeight \\+ 24\\)"
   OR NOT main_qml MATCHES "Layout.minimumWidth: 44"
   OR main_qml MATCHES "applicationIconName \\|\\| \"dialog-warning-symbolic\""
   OR NOT main_qml MATCHES "id: actionRail"
   OR NOT main_qml MATCHES "Layout.maximumWidth: 44"
   OR NOT main_qml MATCHES "anchors.right: parent.right"
   OR NOT main_qml MATCHES "Layout.minimumHeight: actionButtons.implicitHeight"
   OR NOT main_qml MATCHES "maximumLineCount: 8")
    message(FATAL_ERROR "Banner cards must retain bounded content sizing and touch-safe actions")
endif()
if(main_qml MATCHES "notificationHistory.clear\\(NotificationManager.Notifications.ClearExpired\\)")
    message(FATAL_ERROR "Clear history must dismiss active alerts, not only expired notifications")
endif()
string(FIND "${main_qml}" "notificationHistory.close(notificationHistory.index(row, 0))" clear_all_position)
if(clear_all_position LESS 0)
    message(FATAL_ERROR "Explicit per-notification history dismissal is missing")
endif()
string(REGEX MATCH "function isPriorityNotificationCandidate[^}]+}" priority_policy "${main_qml}")
if(priority_policy MATCHES "HasDefaultActionRole|ActionNamesRole")
    message(FATAL_ERROR "Normal actionable notifications must not be promoted to banners")
endif()
if(NOT priority_policy MATCHES "CriticalUrgency" OR NOT priority_policy MATCHES "isFreshLogoutCancellation")
    message(FATAL_ERROR "Critical and logout-cancellation alerts must keep their banner route")
endif()
file(READ "${SOURCE_DIR}/qml/ControlCenterPage.qml" control_center_qml)
file(READ "${SOURCE_DIR}/qml/OrganizedTrayPage.qml" organized_tray_qml)
file(READ "${SOURCE_DIR}/qml/NotificationHistoryPage.qml" notification_history_qml)
file(READ "${SOURCE_DIR}/main.xml" config_xml)
if(NOT notification_history_qml MATCHES "property bool detailsExpanded: false"
   OR NOT notification_history_qml MATCHES "summaryLabel.truncated \\|\\| bodyLabel.truncated"
   OR NOT notification_history_qml MATCHES "i18n\\(\"Show more\"\\)"
   OR NOT notification_history_qml MATCHES "i18n\\(\"Show less\"\\)"
   OR NOT notification_history_qml MATCHES "radius: height / 2")
    message(FATAL_ERROR "Truncated notification history cards must expand in place")
endif()
foreach(session_toggle showLogout showRestart showShutdown)
    if(NOT config_xml MATCHES "<entry name=\"${session_toggle}\" type=\"Bool\">[\n\r ]*<label>[^<]+</label>[\n\r ]*<default>true</default>")
        message(FATAL_ERROR "Session control ${session_toggle} must default enabled")
    endif()
endforeach()
if(NOT config_xml MATCHES "<entry name=\"showSwitchUser\" type=\"Bool\">[\n\r ]*<label>[^<]+</label>[\n\r ]*<default>false</default>")
    message(FATAL_ERROR "Switch User must remain opt-in")
endif()
file(READ "${SOURCE_DIR}/qml/ExpandedRepresentation.qml" expanded_qml)
string(FIND "${expanded_qml}" "id: sessionActions" session_actions_position)
if(session_actions_position LESS 0 OR control_center_qml MATCHES "id: sessionActions")
    message(FATAL_ERROR "Session controls belong in the popup header, not content")
endif()
file(READ "${SOURCE_DIR}/metadata.json" root_metadata)
file(READ "${SOURCE_DIR}/src/metadata.json" plugin_metadata)

if(NOT EXISTS "${SOURCE_DIR}/assets/studio.warbler.temperance.png")
    message(FATAL_ERROR "The Temperance widget icon is missing")
endif()

if(main_qml MATCHES "criticalNotifications")
    message(FATAL_ERROR "The important-alert popup references the obsolete criticalNotifications model")
endif()

if(NOT main_qml MATCHES "id: priorityNotifications")
    message(FATAL_ERROR "The important-alert priority model is missing")
endif()

if(NOT main_qml MATCHES "minimizePriorityNotification")
    message(FATAL_ERROR "Important alerts must support keeping an item for review")
endif()

if(NOT main_qml MATCHES "component BannerActionButton: Item"
   OR NOT main_qml MATCHES "ctx.lineCap = \"round\""
   OR NOT main_qml MATCHES "width: 28[\n\r ]+height: 28[\n\r ]+radius: height / 2"
   OR NOT main_qml MATCHES "Layout.maximumWidth: 44"
   OR NOT main_qml MATCHES "anchors.centerIn: parent"
   OR main_qml MATCHES "PlasmaComponents.ToolTip \\{ text: bannerActionButton.text \\}"
   OR NOT main_qml MATCHES "text: i18n\\(\"Keep for review\"\\)")
    message(FATAL_ERROR "Important-alert actions must retain touch-safe targets and compact rounded visuals")
endif()

if(NOT main_qml MATCHES "reservedForBanner")
    message(FATAL_ERROR "Banner alerts must not simultaneously enter the ticker")
endif()

string(FIND "${control_center_qml}"
    "implicitHeight: contentLayout.implicitHeight + 36" control_height_position)
if(control_height_position LESS 0)
    message(FATAL_ERROR "Control Center must retain the shared 24 px bottom rhythm")
endif()

string(FIND "${organized_tray_qml}" "title: i18n(\"Apps\")" apps_position)
string(FIND "${organized_tray_qml}" "title: i18n(\"Devices\")" devices_position)
string(FIND "${organized_tray_qml}" "title: i18n(\"System\")" system_position)
if(apps_position LESS 0 OR devices_position LESS apps_position OR system_position LESS devices_position)
    message(FATAL_ERROR "Tray sections must remain ordered Apps, Devices, System")
endif()

if(NOT organized_tray_qml MATCHES "spacing: 24")
    message(FATAL_ERROR "Tray hierarchy must collapse around populated sections")
endif()

if(NOT config_xml MATCHES "<entry name=\"adaptiveWidth\" type=\"Bool\">[\n\r ]*<label>[^<]+</label>[\n\r ]*<default>true</default>")
    message(FATAL_ERROR "Responsive panel sizing must remain enabled by default")
endif()

if(NOT root_metadata STREQUAL plugin_metadata)
    message(FATAL_ERROR "Root and native-plugin metadata must remain identical")
endif()

if(NOT root_metadata MATCHES "\"Icon\"[ ]*:[ ]*\"studio.warbler.temperance\"")
    message(FATAL_ERROR "Widget metadata does not reference the Temperance icon")
endif()
