# SPDX-FileCopyrightText: 2026 carlsonjm
# SPDX-License-Identifier: ISC

file(READ "${SOURCE_DIR}/qml/main.qml" main_qml)
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

if(NOT config_xml MATCHES "<entry name=\"adaptiveWidth\" type=\"Bool\">[\n\r ]*<label>[^<]+</label>[\n\r ]*<default>true</default>")
    message(FATAL_ERROR "Responsive panel sizing must remain enabled by default")
endif()

if(NOT root_metadata STREQUAL plugin_metadata)
    message(FATAL_ERROR "Root and native-plugin metadata must remain identical")
endif()

if(NOT root_metadata MATCHES "\"Icon\"[ ]*:[ ]*\"studio.warbler.temperance\"")
    message(FATAL_ERROR "Widget metadata does not reference the Temperance icon")
endif()
