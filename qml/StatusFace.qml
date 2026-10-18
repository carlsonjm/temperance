/*
    SPDX-FileCopyrightText: 2011 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2020 Konrad Materka <materka@gmail.com>
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami

// What a status icon draws: lifted while a finger is on it, as a touched
// piece lifts elsewhere in the suite, and grown a little under a pointer.
Item {
    property bool touched: false
    property bool hovered: false
    scale: touched ? 1.12 : hovered ? 1.06 : 1
    Behavior on scale {
        id: statusFaceScale
        enabled: Kirigami.Units.longDuration > 0
        NumberAnimation {
            duration: statusFaceScale.targetValue > 1.1 ? 180 : 140
            easing.type: Easing.OutCubic
        }
    }
}
