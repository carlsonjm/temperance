/*
    SPDX-FileCopyrightText: 2026 carlsonjm
    SPDX-License-Identifier: LGPL-2.0-or-later
*/
.pragma library

// Which row of Plasma's brightness displays is the screen a Status Bar stands
// on, so Control Center's slider changes that screen and no other. Plasma
// names a display by an id of its own; where the id carries the output's
// name (eDP-1, DP-2) that settles it, then the monitor's model in its label,
// then built-in against external. With one display, or nothing to go on, it
// is the first row, as before.

// roleOf gives a role's number by its name, -1 for one the model lacks; QML
// passes it, since only QML reaches the model's role names.
function value(model, roleOf, row, name) {
    const r = roleOf(name);
    return r >= 0 ? model.data(model.index(row, 0), r) : undefined;
}

// A connector's name says whether the screen is built in.
function builtIn(outputName) {
    return /^(eDP|LVDS|DSI)/i.test(String(outputName || ""));
}

function rowFor(model, roleOf, outputName, outputModel) {
    const rows = model ? model.rowCount() : 0;
    if (rows < 1) {
        return -1;
    }
    if (rows === 1) {
        return 0;
    }
    const output = String(outputName || "");
    if (output.length > 0) {
        for (let row = 0; row < rows; ++row) {
            const id = String(value(model, roleOf, row, "displayName") || "");
            if (id === output || id.endsWith(":" + output) || id.endsWith("/" + output)
                    || id.endsWith("-" + output) || id.endsWith("_" + output)) {
                return row;
            }
        }
    }
    const monitor = String(outputModel || "").trim().toLowerCase();
    if (monitor.length > 0) {
        for (let row = 0; row < rows; ++row) {
            const label = String(value(model, roleOf, row, "label") || "").toLowerCase();
            if (label.length > 0 && (label.indexOf(monitor) >= 0 || monitor.indexOf(label) >= 0)) {
                return row;
            }
        }
    }
    if (roleOf("isInternal") >= 0 && output.length > 0) {
        const wanted = builtIn(output);
        for (let row = 0; row < rows; ++row) {
            if (Boolean(value(model, roleOf, row, "isInternal")) === wanted) {
                return row;
            }
        }
    }
    return 0;
}
