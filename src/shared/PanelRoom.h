/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Shared by Tettegouche and Temperance. This folder in Tettegouche is where
// the shared code changes; Temperance carries a byte-for-byte copy, which its
// tools/shared.sh records and checks, so one change reaches both.

#pragma once

#include <Plasma/Applet>
#include <Plasma/Containment>
#include <PlasmaQuick/AppletQuickItem>

#include <QList>
#include <QPointF>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <functional>

namespace suite
{
// Which way a widget grows along its panel: Search toward the widgets after
// it, the Status Bar toward those before it.
enum class Toward { After, Before };

// The room a widget has along its panel: up to the nearest widget the panel
// shows on the side it grows toward, less the gap, and never under
// minimumWidth, which is all it has where no widget stands there. Spacers and
// margins are not widgets. Furniture a container paints rather than hosts is
// not in this walk, so there the measurement runs past it; such a container
// holds its widgets to their side, as Shuffle's band does, and that clamp
// decides the width.
inline int panelRoom(Plasma::Applet *self, QQuickItem *visualParent, int minimumWidth, int gap, Toward toward)
{
    minimumWidth = std::max(1, minimumWidth);
    gap = std::max(0, gap);
    if (!self || !visualParent || !visualParent->window() || !self->containment()) {
        return minimumWidth;
    }

    QQuickItem *ownItem = PlasmaQuick::AppletQuickItem::itemForApplet(self);
    if (!ownItem || ownItem->window() != visualParent->window()) {
        ownItem = visualParent;
    }

    const qreal ownLeft = ownItem->mapToScene(QPointF(0, 0)).x();
    const qreal ownRight = ownItem->mapToScene(QPointF(ownItem->width(), 0)).x();
    qreal nearest = -1;
    for (Plasma::Applet *applet : self->containment()->applets()) {
        if (!applet || applet == self || applet->destroyed()) {
            continue;
        }
        const QString pluginId = applet->pluginMetaData().pluginId();
        if (pluginId == QLatin1String("org.kde.plasma.panelspacer")
            || pluginId == QLatin1String("org.kde.plasma.marginsseparator")
            || !PlasmaQuick::AppletQuickItem::hasItemForApplet(applet)) {
            continue;
        }
        auto *item = PlasmaQuick::AppletQuickItem::itemForApplet(applet);
        if (!item || !item->isVisible() || item->window() != ownItem->window()) {
            continue;
        }
        if (toward == Toward::After) {
            const qreal itemLeft = item->mapToScene(QPointF(0, 0)).x();
            if (itemLeft >= ownRight - 1 && (nearest < 0 || itemLeft < nearest)) {
                nearest = itemLeft;
            }
        } else {
            const qreal itemRight = item->mapToScene(QPointF(item->width(), 0)).x();
            if (itemRight <= ownLeft + 1 && itemRight > nearest) {
                nearest = itemRight;
            }
        }
    }
    if (nearest < 0) {
        return minimumWidth;
    }
    const qreal room = toward == Toward::After ? nearest - ownLeft - gap : ownRight - nearest - gap;
    return std::max(minimumWidth, static_cast<int>(std::floor(room)));
}

// Follows what that room is measured by and calls changed at each move: the
// widgets on the panel, each with the items that hold it, since Plasma moves
// a widget's containers while the widget keeps its own place in them, and
// widgets added to or taken from the panel.
class PanelWatch
{
public:
    void follow(Plasma::Applet *self, QQuickItem *visualParent, const std::function<void()> &changed)
    {
        followItem(self, visualParent, changed);
        Plasma::Containment *panel = self->containment();
        if (!panel) {
            return;
        }
        if (!m_followingPanel) {
            m_followingPanel = true;
            QObject::connect(panel, &Plasma::Containment::appletAdded, self, [self, changed](Plasma::Applet *) {
                QTimer::singleShot(0, self, changed);
            });
            QObject::connect(panel, &Plasma::Containment::appletRemoved, self, [changed](Plasma::Applet *) {
                changed();
            });
        }
        for (Plasma::Applet *applet : panel->applets()) {
            if (applet && PlasmaQuick::AppletQuickItem::hasItemForApplet(applet)) {
                followItem(self, PlasmaQuick::AppletQuickItem::itemForApplet(applet), changed);
            }
        }
    }

private:
    void followItem(QObject *context, QQuickItem *item, const std::function<void()> &changed)
    {
        m_items.removeIf([](const QPointer<QQuickItem> &followed) {
            return followed.isNull();
        });
        for (; item; item = item->parentItem()) {
            if (m_items.contains(item)) {
                continue;
            }
            m_items.append(item);
            QObject::connect(item, &QQuickItem::xChanged, context, changed);
            QObject::connect(item, &QQuickItem::widthChanged, context, changed);
            QObject::connect(item, &QQuickItem::visibleChanged, context, changed);
            QObject::connect(item, &QQuickItem::windowChanged, context, changed);
            QObject::connect(item, &QQuickItem::parentChanged, context, changed);
        }
    }

    QList<QPointer<QQuickItem>> m_items;
    bool m_followingPanel = false;
};
}
