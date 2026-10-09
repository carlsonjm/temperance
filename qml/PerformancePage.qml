/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: GPL-2.0-or-later
*/
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.plasmoid

// The Z13's performance page, opened from Control Center's profile entry and
// shown only where the z13ctl-plus helper is found. Profiles apply at a tap;
// the charge limit and the automatic switch apply as they change. A profile's
// own settings are staged here and reach the profile only through Save, which
// is the one place a profile is ever saved.
Item {
    id: page

    StatusColors {
        id: tone
        theme: page.Kirigami.Theme
    }

    required property color accentColor
    required property real maximumHeight
    readonly property int compactSpacing: 8
    readonly property int standardSpacing: 12
    readonly property int surfaceSpacing: 24
    readonly property color accentTextColor: (accentColor.r * 0.299 + accentColor.g * 0.587 + accentColor.b * 0.114) > 0.58
        ? "#102729" : "#FFFFFF"

    readonly property var presets: Plasmoid.performancePresets
    readonly property string activePreset: Plasmoid.activePerformancePreset
    readonly property bool busy: Plasmoid.performanceBusy
    readonly property var refreshRates: Plasmoid.screenRefresh ? Plasmoid.screenRefresh.rates : []
    readonly property int refreshRate: Plasmoid.screenRefresh ? Plasmoid.screenRefresh.rate : 0
    readonly property bool refreshHeld: Plasmoid.screenRefresh ? Plasmoid.screenRefresh.held : false
    readonly property int slowestRate: refreshRates.length > 0 ? refreshRates[0] : 0
    readonly property int fastestRate: refreshRates.length > 0 ? refreshRates[refreshRates.length - 1] : 0

    implicitWidth: Kirigami.Units.gridUnit * 24
    implicitHeight: Math.min(maximumHeight, pageContent.implicitHeight + standardSpacing + surfaceSpacing)

    readonly property var profileChoices: [
        { value: "quiet", label: i18n("Quiet") },
        { value: "balanced", label: i18n("Balanced") },
        { value: "performance", label: i18n("Performance") }
    ]
    // The four AMD energy preferences, from most saving to fastest.
    readonly property var eppChoices: [
        { value: "power", label: i18n("Save most") },
        { value: "balance_power", label: i18n("Save some") },
        { value: "balance_performance", label: i18n("Respond faster") },
        { value: "performance", label: i18n("Fastest") }
    ]
    // Sustained, short boost and fast boost, as the helper measured them for
    // each profile, shown while a profile leaves its limits to the firmware.
    readonly property var profileLimits: ({
        quiet: [40, 55, 55],
        balanced: [52, 71, 70],
        performance: [70, 86, 86]
    })
    // The sliders stop at 70 W, as Z13GUI+'s do: above 75 W of sustained
    // power the helper asks for force, which this page never sends.
    readonly property int wattsMin: 5
    readonly property int wattsMax: 70
    readonly property int sustainedMax: 75
    // Z13GUI+'s starting curve (Apache-2.0), drawn while the firmware runs
    // the fans and taken up when a point is first dragged.
    readonly property var defaultFanCurve: [
        { temp: 35, pwm: 0 }, { temp: 45, pwm: 25 }, { temp: 50, pwm: 50 }, { temp: 60, pwm: 80 },
        { temp: 70, pwm: 120 }, { temp: 80, pwm: 170 }, { temp: 90, pwm: 220 }, { temp: 100, pwm: 255 }
    ]

    // Profile editing stays folded away until asked for.
    property bool editorOpen: false
    // The profile being edited and what is staged for it.
    property string editName: ""
    property string savedProfile: ""
    property string stagedProfile: ""
    property string savedEpp: ""
    property string stagedEpp: ""
    // Empty while the limits are the profile's own.
    property var stagedLimits: []
    property bool limitsSavedDefault: true
    property bool tdpDirty: false
    property var stagedFan: []
    property bool fanSavedAuto: true
    property bool fanAuto: true
    property bool fanDirty: false
    property int fanAxisMin: 30
    property int fanAxisMax: 110
    readonly property bool limitsDefault: stagedLimits.length !== 3
    readonly property bool profileDirty: stagedProfile !== savedProfile
    readonly property bool eppDirty: stagedEpp !== savedEpp
    readonly property bool dirty: profileDirty || eppDirty || tdpDirty || fanDirty
    property bool saving: false
    property string saveError: ""
    // The charge limit while it is dragged, until the helper reports it.
    property int chargeDraft: 0

    function copyCurve(points) {
        return points.map(point => ({ temp: point.temp, pwm: point.pwm }));
    }

    function parseCurve(text) {
        const parts = String(text || "").split(",");
        if (parts.length !== 8) return null;
        const points = parts.map(part => {
            const pair = part.split(":");
            return { temp: Number(pair[0]), pwm: Number(pair[1]) };
        });
        return points.every(point => isFinite(point.temp) && isFinite(point.pwm)) ? points : null;
    }

    function loadEditor(name) {
        const settings = (name && Plasmoid.performancePresetSettings[name]) || {};
        editName = name || "";
        savedProfile = settings.profile || "";
        stagedProfile = savedProfile;
        savedEpp = settings.epp || "";
        stagedEpp = savedEpp;
        limitsSavedDefault = settings.pl1 === undefined;
        stagedLimits = limitsSavedDefault ? [] : [Number(settings.pl1), Number(settings.pl2), Number(settings.pl3)];
        tdpDirty = false;
        const curve = parseCurve(settings.fanCurve);
        fanSavedAuto = curve === null;
        fanAuto = fanSavedAuto;
        stagedFan = curve || copyCurve(defaultFanCurve);
        fanAxisMin = Math.max(0, Math.min(30, Math.floor(stagedFan[0].temp / 10) * 10));
        fanAxisMax = Math.min(120, Math.max(110, Math.ceil(stagedFan[7].temp / 10) * 10));
        fanDirty = false;
        saveError = "";
    }

    // Opens on the profile in use, unless an unsaved edit is waiting, which
    // also keeps the editor unfolded.
    function open() {
        if (!dirty || presets.indexOf(editName) < 0) {
            loadEditor(presets.indexOf(activePreset) >= 0 ? activePreset : (presets.length > 0 ? presets[0] : ""));
        }
        editorOpen = dirty;
    }

    function limitAt(index) {
        if (!limitsDefault) return stagedLimits[index];
        const own = profileLimits[stagedProfile];
        return own ? own[index] : 0;
    }

    function setLimit(index, watts) {
        const next = [limitAt(0), limitAt(1), limitAt(2)].map(value => Math.max(wattsMin, value));
        next[index] = watts;
        next[0] = Math.min(sustainedMax, next[0]);
        stagedLimits = next;
        tdpDirty = true;
    }

    function resetLimits() {
        stagedLimits = [];
        tdpDirty = !limitsSavedDefault;
    }

    // Adapted from Z13GUI+'s fan curve editor (Apache-2.0): the dragged point
    // pushes its neighbours, so temperatures keep rising and speeds never fall,
    // and it stops short of the edges so the others always have room.
    function moveFanPoint(index, temp, pwm) {
        const points = copyCurve(stagedFan);
        const last = points.length - 1;
        points[index].temp = Math.max(fanAxisMin + index, Math.min(fanAxisMax - (last - index), temp));
        points[index].pwm = Math.max(0, Math.min(255, pwm));
        for (let i = index + 1; i <= last; ++i) {
            if (points[i].temp <= points[i - 1].temp) points[i].temp = points[i - 1].temp + 1;
            if (points[i].pwm < points[i - 1].pwm) points[i].pwm = points[i - 1].pwm;
        }
        for (let i = index - 1; i >= 0; --i) {
            if (points[i].temp >= points[i + 1].temp) points[i].temp = points[i + 1].temp - 1;
            if (points[i].pwm > points[i + 1].pwm) points[i].pwm = points[i + 1].pwm;
        }
        stagedFan = points;
        fanAuto = false;
        fanDirty = true;
    }

    function resetFan() {
        stagedFan = copyCurve(defaultFanCurve);
        fanAxisMin = 30;
        fanAxisMax = 110;
        fanAuto = true;
        fanDirty = !fanSavedAuto;
    }

    function save() {
        if (!dirty || busy || saving || editName === "") return;
        const settings = {};
        if (profileDirty) settings.profile = stagedProfile;
        if (eppDirty) settings.epp = stagedEpp;
        if (tdpDirty) {
            if (limitsDefault) {
                settings.tdpReset = true;
            } else {
                settings.pl1 = Math.round(stagedLimits[0]);
                settings.pl2 = Math.round(stagedLimits[1]);
                settings.pl3 = Math.round(stagedLimits[2]);
            }
        }
        if (fanDirty) {
            if (fanAuto) settings.fanReset = true;
            else settings.fanCurve = stagedFan.map(point => Math.round(point.temp) + ":" + Math.round(point.pwm)).join(",");
        }
        saveError = "";
        saving = true;
        Plasmoid.savePerformancePreset(editName, settings);
    }

    Connections {
        target: Plasmoid
        function onPerformancePresetSaved(name, ok, detail) {
            if (!page.saving) return;
            page.saving = false;
            if (ok) {
                // What was staged is now the profile; the helper's report
                // follows and loads it.
                page.savedProfile = page.stagedProfile;
                page.savedEpp = page.stagedEpp;
                page.limitsSavedDefault = page.limitsDefault;
                page.fanSavedAuto = page.fanAuto;
                page.tdpDirty = false;
                page.fanDirty = false;
            } else {
                page.saveError = detail
                    ? i18n("Couldn't save to %1, which is unchanged: %2", name, detail)
                    : i18n("Couldn't save to %1, which is unchanged.", name);
            }
        }
        function onPerformanceStateChanged() {
            page.chargeDraft = 0;
            if (!page.dirty && !page.saving) page.loadEditor(page.editName);
        }
        function onPerformancePresetsChanged() {
            if (page.presets.indexOf(page.editName) < 0) page.open();
        }
    }

    component DetailLabel: PlasmaComponents.Label {
        Layout.fillWidth: true
        opacity: 0.65
        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
        wrapMode: Text.Wrap
    }

    // A choice among a few, lit in the accent while it is the one chosen.
    component ChoicePill: Rectangle {
        id: choice
        required property string label
        property bool selected: false
        signal triggered()
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: 0
        Layout.preferredHeight: 36
        radius: height / 2
        opacity: enabled ? 1 : 0.5
        color: selected ? page.accentColor
            : choiceTap.pressed ? tone.wash(0.18)
            : choiceHover.hovered ? tone.wash(0.12) : tone.wash(0.07)
        border.width: activeFocus ? 1 : 0
        border.color: tone.text
        Behavior on color { ColorAnimation { duration: 120 } }
        PlasmaComponents.Label {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            text: choice.label
            color: choice.selected ? page.accentTextColor : tone.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        activeFocusOnTab: true
        Accessible.role: Accessible.RadioButton
        Accessible.name: label
        Accessible.checked: selected
        Accessible.onPressAction: triggered()
        Keys.onReturnPressed: triggered()
        Keys.onSpacePressed: triggered()
        HoverHandler { id: choiceHover }
        TapHandler { id: choiceTap; onTapped: choice.triggered() }
    }

    // A small grey pill for an action beside a section's name.
    component TextPill: PlasmaComponents.ToolButton {
        id: textPill
        Layout.preferredHeight: 30
        leftPadding: 12
        rightPadding: 12
        contentItem: PlasmaComponents.Label {
            text: textPill.text
            color: tone.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: height / 2
            color: textPill.down ? tone.controlPressed : textPill.hovered ? tone.controlHover : tone.control
            border.width: textPill.visualFocus ? 1 : 0
            border.color: tone.text
            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }

    component AccentSlider: PlasmaComponents.Slider {
        id: accentSlider
        background: Item {
            x: accentSlider.leftPadding
            y: accentSlider.topPadding + Math.round((accentSlider.availableHeight - height) / 2)
            width: accentSlider.availableWidth
            height: 4
            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: tone.wash(0.07)
            }
            Rectangle {
                width: Math.max(height, accentSlider.visualPosition * parent.width)
                height: parent.height
                radius: height / 2
                color: page.accentColor
            }
        }
        handle: Rectangle {
            x: accentSlider.leftPadding + accentSlider.visualPosition * (accentSlider.availableWidth - width)
            y: accentSlider.topPadding + (accentSlider.availableHeight - height) / 2
            implicitWidth: 16
            implicitHeight: 16
            radius: width / 2
            color: accentSlider.pressed ? Qt.lighter(page.accentColor, 1.18) : page.accentColor
        }
    }

    // One power limit: its name and watts over a slider.
    component LimitSlider: ColumnLayout {
        id: limitRow
        required property int limitIndex
        required property string label
        Layout.fillWidth: true
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            PlasmaComponents.Label {
                Layout.fillWidth: true
                text: limitRow.label
                elide: Text.ElideRight
            }
            PlasmaComponents.Label {
                text: i18n("%1 W", page.limitAt(limitRow.limitIndex))
                font.features: { "tnum": 1 }
            }
        }
        AccentSlider {
            Layout.fillWidth: true
            from: page.wattsMin
            to: page.wattsMax
            stepSize: 1
            snapMode: QQC2.Slider.SnapAlways
            enabled: !page.busy && !page.saving
            value: Math.max(page.wattsMin, Math.min(page.wattsMax, page.limitAt(limitRow.limitIndex)))
            onMoved: page.setLimit(limitRow.limitIndex, Math.round(value))
            Accessible.name: limitRow.label
        }
    }

    PlasmaComponents.ScrollView {
        id: scrollView
        anchors.fill: parent
        background: null
        contentWidth: availableWidth
        contentHeight: pageContent.implicitHeight + page.standardSpacing + page.surfaceSpacing

        ColumnLayout {
            id: pageContent
            // On the popup's margin line, 12 px into the page.
            x: 12
            y: page.standardSpacing
            width: scrollView.availableWidth - 24
            spacing: page.compactSpacing

            // 1. Profiles: choosing one applies it.
            RowLayout {
                Layout.fillWidth: true
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: i18n("Profile")
                    font.weight: Font.Medium
                }
                PlasmaComponents.ComboBox {
                    id: presetPicker
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 9
                    model: page.presets
                    currentIndex: page.presets.indexOf(page.activePreset)
                    enabled: !page.busy
                    Accessible.name: i18n("Profile")
                    onActivated: index => {
                        const name = page.presets[index];
                        if (name !== page.activePreset) Plasmoid.applyPerformancePreset(name);
                    }
                    // A refused apply leaves the profile in use showing.
                    Connections {
                        target: page
                        function onActivePresetChanged() { presetPicker.currentIndex = page.presets.indexOf(page.activePreset); }
                        function onBusyChanged() { if (!page.busy) presetPicker.currentIndex = page.presets.indexOf(page.activePreset); }
                    }
                }
            }

            // 2. Charge limit: the battery stops charging here. It belongs to
            // the machine, not to a profile.
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: page.surfaceSpacing - page.compactSpacing
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: i18n("Charge limit")
                    font.weight: Font.Medium
                }
                PlasmaComponents.Label {
                    readonly property int percent: Math.round(chargeSlider.value)
                    text: percent === 100 ? i18n("%1% · Standard", percent)
                        : percent === 80 ? i18n("%1% · Balanced", percent)
                        : percent === 60 ? i18n("%1% · Max life", percent)
                        : i18n("%1%", percent)
                    font.features: { "tnum": 1 }
                }
            }
            AccentSlider {
                id: chargeSlider
                readonly property int stored: Plasmoid.performanceBatteryLimit > 0 ? Plasmoid.performanceBatteryLimit : 100
                Layout.fillWidth: true
                from: 40
                to: 100
                stepSize: 5
                snapMode: QQC2.Slider.SnapAlways
                enabled: !page.busy
                Accessible.name: i18n("Charge limit")
                function commit() {
                    if (page.chargeDraft > 0 && page.chargeDraft !== chargeSlider.stored)
                        Plasmoid.setPerformanceBatteryLimit(page.chargeDraft);
                }
                onMoved: {
                    page.chargeDraft = Math.round(chargeSlider.value);
                    if (!chargeSlider.pressed) chargeSlider.commit();
                }
                onPressedChanged: if (!chargeSlider.pressed) chargeSlider.commit()
            }
            Binding {
                target: chargeSlider
                property: "value"
                value: page.chargeDraft > 0 ? page.chargeDraft : chargeSlider.stored
                when: !chargeSlider.pressed
            }
            DetailLabel {
                text: i18n("The battery stops charging here. A lower limit keeps it healthy longer.")
            }

            // 3. Refresh rate: one switch between the screen's slowest and
            // fastest rates, following the rate in use. The profile sets it;
            // a rate switched here is Manual until the profile changes.
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: page.surfaceSpacing - page.compactSpacing
                visible: page.refreshRates.length > 1
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: i18n("%1 Hz refresh rate", page.fastestRate)
                    font.weight: Font.Medium
                    wrapMode: Text.Wrap
                }
                PlasmaComponents.Label {
                    visible: page.refreshHeld
                    text: i18n("Manual")
                    opacity: 0.65
                }
                PlasmaComponents.Switch {
                    checked: page.refreshRate === page.fastestRate
                    Accessible.name: i18n("%1 Hz refresh rate", page.fastestRate)
                    onToggled: Plasmoid.screenRefresh.choose(checked ? page.fastestRate : page.slowestRate)
                }
            }
            DetailLabel {
                visible: page.refreshRates.length > 1
                text: i18n("Your profile sets the rate. One switched here stays until the profile changes.")
            }

            // 4. Automatic switching between the plugged-in and battery profiles.
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: page.surfaceSpacing - page.compactSpacing
                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    text: i18n("Switch when plugged in or unplugged")
                    font.weight: Font.Medium
                    wrapMode: Text.Wrap
                }
                PlasmaComponents.Switch {
                    checked: Plasmoid.performanceAutoSwitch
                    enabled: !page.busy
                    Accessible.name: i18n("Switch when plugged in or unplugged")
                    onToggled: Plasmoid.setPerformancePowerPolicy(checked, Plasmoid.performanceAcPreset, Plasmoid.performanceBatteryPreset)
                }
            }
            Repeater {
                model: [
                    { label: i18n("Plugged in"), battery: false },
                    { label: i18n("On battery"), battery: true }
                ]
                delegate: RowLayout {
                    id: sourceRow
                    required property var modelData
                    Layout.fillWidth: true
                    enabled: Plasmoid.performanceAutoSwitch && !page.busy
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: sourceRow.modelData.label
                    }
                    PlasmaComponents.ComboBox {
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 9
                        model: page.presets
                        currentIndex: page.presets.indexOf(sourceRow.modelData.battery
                            ? Plasmoid.performanceBatteryPreset : Plasmoid.performanceAcPreset)
                        Accessible.name: sourceRow.modelData.label
                        onActivated: index => {
                            const name = page.presets[index];
                            if (sourceRow.modelData.battery) {
                                Plasmoid.setPerformancePowerPolicy(true, Plasmoid.performanceAcPreset, name);
                            } else {
                                Plasmoid.setPerformancePowerPolicy(true, name, Plasmoid.performanceBatteryPreset);
                            }
                        }
                    }
                }
            }

            // 5. A profile's own settings, staged until Save, folded until asked for.
            RowLayout {
                id: editHeader
                Layout.fillWidth: true
                Layout.topMargin: page.surfaceSpacing - page.compactSpacing
                PlasmaComponents.ToolButton {
                    Layout.fillWidth: true
                    text: i18n("Edit a profile")
                    contentItem: RowLayout {
                        spacing: page.compactSpacing
                        PlasmaComponents.Label {
                            Layout.fillWidth: true
                            text: i18n("Edit a profile")
                            font.weight: Font.Medium
                        }
                        Kirigami.Icon {
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: Kirigami.Units.iconSizes.small
                            source: page.editorOpen ? "arrow-up-symbolic" : "arrow-down-symbolic"
                        }
                    }
                    background: null
                    Accessible.role: Accessible.Button
                    Accessible.name: i18n("Edit a profile")
                    Accessible.description: page.editorOpen ? i18n("Folds the profile editor away") : i18n("Shows the profile editor")
                    onClicked: page.editorOpen = !page.editorOpen
                }
                PlasmaComponents.ComboBox {
                    visible: page.editorOpen
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 9
                    model: page.presets
                    currentIndex: page.presets.indexOf(page.editName)
                    enabled: !page.saving
                    Accessible.name: i18n("Profile to edit")
                    onActivated: index => page.loadEditor(page.presets[index])
                }
            }

            ColumnLayout {
                visible: page.editorOpen && page.editName !== ""
                Layout.fillWidth: true
                spacing: page.compactSpacing
                enabled: !page.saving

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.topMargin: page.compactSpacing
                    text: i18n("Performance profile")
                    opacity: 0.8
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: page.compactSpacing
                    Repeater {
                        model: page.profileChoices
                        delegate: ChoicePill {
                            required property var modelData
                            label: modelData.label
                            selected: page.stagedProfile === modelData.value
                            onTriggered: page.stagedProfile = modelData.value
                        }
                    }
                }

                PlasmaComponents.Label {
                    Layout.fillWidth: true
                    Layout.topMargin: page.compactSpacing
                    text: i18n("Energy use")
                    opacity: 0.8
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: page.compactSpacing
                    rowSpacing: page.compactSpacing
                    Repeater {
                        model: page.eppChoices
                        delegate: ChoicePill {
                            required property var modelData
                            label: modelData.label
                            selected: page.stagedEpp === modelData.value
                            onTriggered: page.stagedEpp = modelData.value
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: page.compactSpacing
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: page.limitsDefault ? i18n("Power limits · the profile's own") : i18n("Power limits")
                        opacity: 0.8
                        elide: Text.ElideRight
                    }
                    TextPill {
                        visible: !page.limitsDefault
                        text: i18n("Reset")
                        Accessible.description: i18n("Return the power limits to the profile's own")
                        onClicked: page.resetLimits()
                    }
                }
                LimitSlider { limitIndex: 0; label: i18n("Sustained") }
                LimitSlider { limitIndex: 1; label: i18n("Short boost") }
                LimitSlider { limitIndex: 2; label: i18n("Fast boost") }
                DetailLabel {
                    visible: page.limitAt(0) > page.wattsMax || page.limitAt(1) > page.wattsMax || page.limitAt(2) > page.wattsMax
                    text: i18n("A limit above %1 W stays as it is until you move its slider.", page.wattsMax)
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: page.compactSpacing
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: page.fanAuto ? i18n("Fan curve · automatic") : i18n("Fan curve")
                        opacity: 0.8
                        elide: Text.ElideRight
                    }
                    TextPill {
                        visible: !page.fanAuto
                        text: i18n("Reset")
                        Accessible.description: i18n("Return the fans to automatic")
                        onClicked: page.resetFan()
                    }
                }

                // Eight points, temperature across and fan speed up. Drag a
                // point to move it; its neighbours make way.
                Item {
                    id: fanGraph
                    readonly property real padLeft: 36
                    readonly property real padRight: 10
                    readonly property real padTop: 10
                    readonly property real padBottom: 22
                    readonly property real plotWidth: Math.max(1, width - padLeft - padRight)
                    readonly property real plotHeight: Math.max(1, height - padTop - padBottom)
                    property int dragIndex: -1
                    Layout.fillWidth: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 10
                    Accessible.role: Accessible.Graphic
                    Accessible.name: i18n("Fan curve")

                    function xFor(temp) {
                        return padLeft + (temp - page.fanAxisMin) / (page.fanAxisMax - page.fanAxisMin) * plotWidth;
                    }
                    function yFor(pwm) {
                        return padTop + (1 - pwm / 255) * plotHeight;
                    }
                    function tempAt(x) {
                        return Math.round(page.fanAxisMin + (x - padLeft) / plotWidth * (page.fanAxisMax - page.fanAxisMin));
                    }
                    function pwmAt(y) {
                        return Math.round((1 - (y - padTop) / plotHeight) * 255);
                    }
                    // The point nearest a touch, within a fingertip.
                    function pointNear(x, y) {
                        let best = -1;
                        let bestDistance = 26 * 26;
                        for (let i = 0; i < page.stagedFan.length; ++i) {
                            const dx = x - xFor(page.stagedFan[i].temp);
                            const dy = y - yFor(page.stagedFan[i].pwm);
                            const distance = dx * dx + dy * dy;
                            if (distance < bestDistance) {
                                bestDistance = distance;
                                best = i;
                            }
                        }
                        return best;
                    }

                    onWidthChanged: fanCanvas.requestPaint()
                    onHeightChanged: fanCanvas.requestPaint()
                    onDragIndexChanged: fanCanvas.requestPaint()

                    Connections {
                        target: page
                        function onStagedFanChanged() { fanCanvas.requestPaint(); }
                        function onFanAutoChanged() { fanCanvas.requestPaint(); }
                        function onFanAxisMinChanged() { fanCanvas.requestPaint(); }
                        function onFanAxisMaxChanged() { fanCanvas.requestPaint(); }
                        function onAccentColorChanged() { fanCanvas.requestPaint(); }
                    }
                    Connections {
                        target: tone
                        function onDarkChanged() { fanCanvas.requestPaint(); }
                        function onInkChanged() { fanCanvas.requestPaint(); }
                    }

                    Canvas {
                        id: fanCanvas
                        anchors.fill: parent
                        onPaint: {
                            const ctx = getContext("2d");
                            ctx.reset();
                            const graph = fanGraph;
                            const left = graph.padLeft;
                            const right = graph.padLeft + graph.plotWidth;
                            const top = graph.padTop;
                            const bottom = graph.padTop + graph.plotHeight;

                            ctx.lineWidth = 1;
                            ctx.font = "11px sans-serif";
                            ctx.strokeStyle = tone.wash(0.08);
                            ctx.fillStyle = tone.wash(0.55);
                            ctx.textAlign = "center";
                            ctx.textBaseline = "top";
                            for (let temp = Math.ceil(page.fanAxisMin / 10) * 10; temp <= page.fanAxisMax; temp += 10) {
                                const x = Math.round(graph.xFor(temp)) + 0.5;
                                ctx.beginPath();
                                ctx.moveTo(x, top);
                                ctx.lineTo(x, bottom);
                                ctx.stroke();
                                if (temp % 20 === 0) ctx.fillText(temp + "°", x, bottom + 6);
                            }
                            ctx.textAlign = "right";
                            ctx.textBaseline = "middle";
                            for (let percent = 0; percent <= 100; percent += 25) {
                                const y = Math.round(graph.yFor(percent * 2.55)) + 0.5;
                                ctx.beginPath();
                                ctx.moveTo(left, y);
                                ctx.lineTo(right, y);
                                ctx.stroke();
                                if (percent % 50 === 0) ctx.fillText(percent + "%", left - 6, y);
                            }

                            const points = page.stagedFan;
                            if (points.length !== 8) return;
                            const accent = page.accentColor;
                            ctx.globalAlpha = page.fanAuto ? 0.4 : 1;

                            ctx.beginPath();
                            ctx.moveTo(graph.xFor(points[0].temp), bottom);
                            for (let i = 0; i < points.length; ++i) ctx.lineTo(graph.xFor(points[i].temp), graph.yFor(points[i].pwm));
                            ctx.lineTo(graph.xFor(points[points.length - 1].temp), bottom);
                            ctx.closePath();
                            ctx.fillStyle = Qt.rgba(accent.r, accent.g, accent.b, 0.16);
                            ctx.fill();

                            ctx.beginPath();
                            for (let i = 0; i < points.length; ++i) {
                                const x = graph.xFor(points[i].temp);
                                const y = graph.yFor(points[i].pwm);
                                if (i === 0) ctx.moveTo(x, y);
                                else ctx.lineTo(x, y);
                            }
                            ctx.strokeStyle = accent;
                            ctx.lineWidth = 2;
                            ctx.lineJoin = "round";
                            ctx.stroke();

                            for (let i = 0; i < points.length; ++i) {
                                const radius = i === graph.dragIndex ? 8 : 6;
                                ctx.beginPath();
                                ctx.arc(graph.xFor(points[i].temp), graph.yFor(points[i].pwm), radius, 0, Math.PI * 2);
                                ctx.fillStyle = i === graph.dragIndex ? Qt.lighter(accent, 1.18) : accent;
                                ctx.fill();
                                ctx.lineWidth = 2;
                                ctx.strokeStyle = tone.surface;
                                ctx.stroke();
                            }
                            ctx.globalAlpha = 1;
                        }
                    }

                    // Holds the drag against the page's scrolling once a point
                    // is taken; a touch away from every point scrolls the page.
                    MouseArea {
                        anchors.fill: parent
                        preventStealing: true
                        onPressed: mouse => {
                            const index = fanGraph.pointNear(mouse.x, mouse.y);
                            if (index < 0) {
                                mouse.accepted = false;
                                return;
                            }
                            fanGraph.dragIndex = index;
                        }
                        onPositionChanged: mouse => {
                            if (fanGraph.dragIndex >= 0)
                                page.moveFanPoint(fanGraph.dragIndex, fanGraph.tempAt(mouse.x), fanGraph.pwmAt(mouse.y));
                        }
                        onReleased: fanGraph.dragIndex = -1
                        onCanceled: fanGraph.dragIndex = -1
                    }
                }
                DetailLabel {
                    text: {
                        if (fanGraph.dragIndex >= 0 && fanGraph.dragIndex < page.stagedFan.length) {
                            const point = page.stagedFan[fanGraph.dragIndex];
                            return i18n("%1 °C · fans at %2%", point.temp, Math.round(point.pwm / 2.55));
                        }
                        return page.fanAuto
                            ? i18n("The firmware runs the fans. Drag a point to set your own curve.")
                            : i18n("Drag a point to change it. Fans speed up as the chip warms.");
                    }
                }

                PlasmaComponents.Label {
                    visible: page.saveError !== ""
                    Layout.fillWidth: true
                    Layout.topMargin: page.compactSpacing
                    text: page.saveError
                    color: tone.errorText
                    wrapMode: Text.Wrap
                }

                Rectangle {
                    id: saveButton
                    readonly property bool ready: page.dirty && !page.busy && !page.saving
                    Layout.fillWidth: true
                    Layout.topMargin: page.compactSpacing
                    Layout.preferredHeight: 44
                    radius: height / 2
                    opacity: ready || page.saving ? 1 : 0.45
                    color: !ready ? tone.wash(0.07)
                        : saveTap.pressed ? Qt.darker(page.accentColor, 1.08) : page.accentColor
                    border.width: activeFocus ? 1 : 0
                    border.color: tone.text
                    Behavior on color { ColorAnimation { duration: 120 } }
                    PlasmaComponents.Label {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        text: page.saving ? i18n("Saving to %1…", page.editName) : i18n("Save to %1", page.editName)
                        color: saveButton.ready ? page.accentTextColor : tone.text
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    activeFocusOnTab: ready
                    Accessible.role: Accessible.Button
                    Accessible.name: i18n("Save to %1", page.editName)
                    Accessible.onPressAction: page.save()
                    Keys.onReturnPressed: page.save()
                    Keys.onSpacePressed: page.save()
                    TapHandler { id: saveTap; enabled: saveButton.ready; onTapped: page.save() }
                }
            }
        }
    }
}
