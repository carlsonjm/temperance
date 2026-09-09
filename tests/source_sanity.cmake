# SPDX-FileCopyrightText: 2026 carlsonjm
# SPDX-License-Identifier: ISC

file(READ "${SOURCE_DIR}/qml/main.qml" main_qml)
file(READ "${SOURCE_DIR}/qml/ControlCenterPage.qml" control_center_qml)
file(READ "${SOURCE_DIR}/qml/OrganizedTrayPage.qml" organized_tray_qml)
file(READ "${SOURCE_DIR}/main.xml" config_xml)
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

if(NOT main_qml MATCHES "window-minimize-symbolic")
    message(FATAL_ERROR "The important-alert minimize control is missing")
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
