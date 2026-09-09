/* SPDX-License-Identifier: GPL-2.0-or-later */
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCMUtils
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels
import org.kde.kquickcontrols as KQC
import org.kde.plasma.plasmoid

KCMUtils.ScrollViewKCM {
    id: page

    property color cfg_accentColor
    property list<string> cfg_controlCenterItems: []
    property bool cfg_showPinButton
    property int cfg_compactWidth
    property bool cfg_adaptiveWidth
    property bool cfg_showNotifications
    property bool cfg_showWeather
    property string cfg_temperatureUnit
    property bool cfg_useOnlineWeatherFallback
    property string cfg_weatherApplication
    property bool cfg_showPriorityBanners
    property int cfg_priorityAlertDuration

    readonly property var reservedItems: [
        "org.kde.plasma.notifications",
        "org.kde.plasma.volume",
        "org.kde.plasma.brightness",
        "org.kde.plasma.battery",
        "org.kde.plasma.weather"
    ]

    function setControlCenterItem(itemId, enabled) {
        const next = Array.from(cfg_controlCenterItems);
        const index = next.indexOf(itemId);
        if (enabled && index === -1) next.push(itemId);
        if (!enabled && index !== -1) next.splice(index, 1);
        cfg_controlCenterItems = next;
    }

    header: Kirigami.FormLayout {
        KQC.ColorButton {
            Kirigami.FormData.label: i18n("Highlight color:")
            color: page.cfg_accentColor
            showAlphaChannel: false
            dialogTitle: i18n("Choose highlight color")
            onAccepted: selectedColor => page.cfg_accentColor = selectedColor
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: i18n("Popup controls:")
            text: i18n("Show Keep Open pin")
            checked: page.cfg_showPinButton
            onToggled: page.cfg_showPinButton = checked
        }

        QQC2.SpinBox {
            Kirigami.FormData.label: i18n("Manual width:")
            from: 320
            to: 600
            stepSize: 10
            editable: true
            enabled: !page.cfg_adaptiveWidth
            value: page.cfg_compactWidth
            onValueModified: page.cfg_compactWidth = value
            textFromValue: (value, locale) => i18n("%1 px", value)
            valueFromText: (text, locale) => parseInt(text, 10) || 400
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: i18n("Responsive layout:")
            text: i18n("Use the live space between neighboring panel widgets")
            checked: page.cfg_adaptiveWidth
            onToggled: page.cfg_adaptiveWidth = checked
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: i18n("Notifications:")
            text: i18n("Show the notification ticker and bell")
            checked: page.cfg_showNotifications
            onToggled: page.cfg_showNotifications = checked
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: i18n("Weather:")
            text: i18n("Show current conditions and temperature")
            checked: page.cfg_showWeather
            onToggled: page.cfg_showWeather = checked
        }

        QQC2.ComboBox {
            id: temperatureUnitChooser
            Kirigami.FormData.label: i18n("Temperature unit:")
            textRole: "label"
            valueRole: "value"
            model: [
                { "label": i18n("Weather widget setting"), "value": "weather" },
                { "label": i18n("Fahrenheit"), "value": "fahrenheit" },
                { "label": i18n("Celsius"), "value": "celsius" }
            ]
            enabled: page.cfg_showWeather
            Component.onCompleted: currentIndex = Math.max(0,
                indexOfValue(page.cfg_temperatureUnit))
            onActivated: page.cfg_temperatureUnit = currentValue
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: i18n("Weather fallback:")
            text: i18n("Use an online fallback when the Weather widget has no temperature")
            checked: page.cfg_useOnlineWeatherFallback
            enabled: page.cfg_showWeather
            onToggled: page.cfg_useOnlineWeatherFallback = checked
        }

        QQC2.TextField {
            Kirigami.FormData.label: i18n("Weather application:")
            text: page.cfg_weatherApplication
            placeholderText: i18n("Desktop entry name")
            enabled: page.cfg_showWeather
            onEditingFinished: page.cfg_weatherApplication = text.trim()
            Accessible.description: i18n("Desktop entry name opened by the weather header button")
        }

        QQC2.CheckBox {
            Kirigami.FormData.label: i18n("System alerts:")
            text: i18n("Show important notification banners")
            checked: page.cfg_showPriorityBanners
            enabled: page.cfg_showNotifications
            onToggled: page.cfg_showPriorityBanners = checked
        }

        QQC2.SpinBox {
            Kirigami.FormData.label: i18n("Banner lifetime:")
            from: 5
            to: 60
            value: page.cfg_priorityAlertDuration
            enabled: page.cfg_showNotifications && page.cfg_showPriorityBanners
            onValueModified: page.cfg_priorityAlertDuration = value
            textFromValue: (value, locale) => i18np("%1 second", "%1 seconds", value)
            valueFromText: (text, locale) => parseInt(text, 10) || 20
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Control Center:")
            text: i18n("Selected tray entries become pills and leave the organized tray.")
            wrapMode: Text.Wrap
            Layout.maximumWidth: Kirigami.Units.gridUnit * 22
        }
    }

    view: ListView {
        id: entriesView
        clip: true
        model: KItemModels.KSortFilterProxyModel {
            filterRoleName: "itemId"
            filterRowCallback: (sourceRow, sourceParent) => {
                const idx = sourceModel.index(sourceRow, 0, sourceParent);
                const itemId = sourceModel.data(idx, filterRole);
                return itemId && page.reservedItems.indexOf(itemId) === -1;
            }
            Component.onCompleted: sourceModel = Plasmoid.configSystemTrayModel
        }

        delegate: QQC2.CheckDelegate {
            id: entry
            required property string itemId
            required property string displayText
            required property var decoration
            width: entriesView.width
            text: displayText
            checked: page.cfg_controlCenterItems.indexOf(itemId) !== -1
            icon.source: decoration
            onToggled: page.setControlCenterItem(itemId, checked)
        }
    }
}
