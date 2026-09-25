/* SPDX-License-Identifier: GPL-2.0-or-later */
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCMUtils
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid

// The calendars whose events the clock's calendar shows. Each is linked by the
// private iCal address its service gives out, which Temperance only reads.
KCMUtils.SimpleKCM {
    id: page

    property list<string> cfg_calendarLinks: []

    readonly property var feeds: Plasmoid.calendarFeeds

    function feedState(link) {
        const normalized = feeds.normalizedLink(link);
        return feeds.feeds.find(feed => feed.link === normalized) ?? null;
    }

    // Written as the clock writes the time: a.m. and p.m., lowercase.
    function timeText(value) {
        return value.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
            .replace("AM", "a.m.").replace("PM", "p.m.");
    }

    function statusText(link) {
        const state = feedState(link);
        if (!state)
            return i18nc("@info a calendar link not yet saved", "Read once you apply");
        if (state.state === "loading")
            return i18nc("@info a linked calendar's status", "Checking…");
        if (state.state === "failed")
            return state.message;
        return i18nc("@info %1 is a time", "Updated %1", timeText(state.updated));
    }

    function addLink() {
        const link = feeds.normalizedLink(linkField.text);
        if (link.length === 0)
            return;
        if (!page.cfg_calendarLinks.some(existing => feeds.normalizedLink(existing) === link))
            page.cfg_calendarLinks = page.cfg_calendarLinks.concat([link]);
        linkField.clear();
    }

    Kirigami.FormLayout {
        ColumnLayout {
            Kirigami.FormData.label: i18n("Calendars:")
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                visible: page.cfg_calendarLinks.length === 0
                text: i18n("No calendars linked")
                opacity: 0.66
            }

            Repeater {
                model: page.cfg_calendarLinks
                delegate: RowLayout {
                    id: linkRow
                    required property string modelData
                    required property int index
                    readonly property var feed: {
                        page.feeds.feeds;
                        return page.feedState(modelData);
                    }
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.largeSpacing

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: linkRow.feed?.name || new URL(linkRow.modelData).hostname
                            elide: Text.ElideRight
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: {
                                page.feeds.feeds;
                                return page.statusText(linkRow.modelData);
                            }
                            font: Kirigami.Theme.smallFont
                            color: linkRow.feed?.state === "failed"
                                ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                            opacity: linkRow.feed?.state === "failed" ? 1 : 0.66
                            elide: Text.ElideRight
                        }
                    }
                    QQC2.Button {
                        icon.name: "list-remove-symbolic"
                        text: i18nc("@action:button", "Remove")
                        display: QQC2.AbstractButton.IconOnly
                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        onClicked: {
                            const next = Array.from(page.cfg_calendarLinks);
                            next.splice(linkRow.index, 1);
                            page.cfg_calendarLinks = next;
                        }
                    }
                }
            }
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Link a calendar:")
            Layout.fillWidth: true

            QQC2.TextField {
                id: linkField
                Layout.fillWidth: true
                Layout.minimumWidth: Kirigami.Units.gridUnit * 16
                placeholderText: i18n("Paste a private calendar address")
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                onAccepted: page.addLink()
            }
            QQC2.Button {
                text: i18nc("@action:button", "Add")
                enabled: page.feeds.normalizedLink(linkField.text).length > 0
                onClicked: page.addLink()
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
            opacity: 0.66
            text: i18n("Google Calendar gives this address as the secret address in iCal format, "
                + "iCloud when a calendar is shared publicly, and Outlook when a calendar is published. "
                + "Temperance only reads the calendar and checks it every 10 minutes. "
                + "The address stays in this computer's settings; anyone who has it can read the calendar.")
        }
    }
}
