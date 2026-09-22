/*
    SPDX-FileCopyrightText: 2026 Jared Carlson
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include <QHash>
#include <QObject>
#include <QString>

#include <optional>

/**
 * Reads a published dock extent, when one is published.
 *
 * A container may place something this applet is not shown as an applet: a
 * dock painted by the containment itself is not in its applet list, so
 * measuring to the nearest applet measures straight past it. Where such a
 * container publishes what it placed, this reports it, and the measurement
 * has a real neighbour instead of an inferred one.
 *
 * Absence is the ordinary case. With the bus name unowned this reports
 * nothing and every measurement is made exactly as it is on a plain panel.
 * Nothing here samples on a timer; the change signal is what it acts on.
 */
class DockExtentReader : public QObject
{
    Q_OBJECT

public:
    explicit DockExtentReader(QObject *parent = nullptr);

    /**
     * The dock's right edge on this output, in that output's own
     * coordinates, or nothing when no dock is published for it.
     */
    std::optional<qreal> rightEdgeFor(const QString &outputName) const;

    /**
     * The dock's left edge on this output, for a component composing on the
     * other side of it.
     */
    std::optional<qreal> leftEdgeFor(const QString &outputName) const;

Q_SIGNALS:
    void extentChanged();

private Q_SLOTS:
    /// Also the handler for the surface's change signal.
    void refresh(const QString &outputName);

private:
    struct Extent {
        qreal left = 0;
        qreal right = 0;
    };

    void onServiceOwned();
    void onServiceLost();

    QHash<QString, Extent> m_extents;
    bool m_available = false;
};
