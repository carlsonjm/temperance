/*
    SPDX-FileCopyrightText: 2015 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QAbstractItemModel>
#include <QPointer>

#include <KConfigWatcher>

#include <Plasma/Containment>

class QQuickItem;

namespace Plasma
{
}
class PlasmoidRegistry;
class PlasmoidModel;
class SystemTraySettings;
class StatusNotifierModel;
class SystemTrayModel;
class SortedSystemTrayModel;
class KJob;

namespace PulseAudioQt
{
class Sink;
}

class SystemTray : public Plasma::Containment
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *systemTrayModel READ sortedSystemTrayModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *configSystemTrayModel READ configSystemTrayModel CONSTANT)
    Q_PROPERTY(QStringList performancePresets READ performancePresets NOTIFY performancePresetsChanged)
    Q_PROPERTY(QString activePerformancePreset READ activePerformancePreset NOTIFY activePerformancePresetChanged)
    Q_PROPERTY(int volumePercent READ volumePercent NOTIFY volumeChanged)
    Q_PROPERTY(bool volumeMuted READ volumeMuted NOTIFY volumeChanged)
    Q_PROPERTY(bool volumeAvailable READ volumeAvailable NOTIFY volumeChanged)

public:
    SystemTray(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
    ~SystemTray() override;

    void init() override;

    void restoreContents(KConfigGroup &group) override;

    QAbstractItemModel *sortedSystemTrayModel();

    QAbstractItemModel *configSystemTrayModel();

    // Invocable utilities
    /**
     * Given an AppletInterface pointer, shows a proper context menu for it
     */
    Q_INVOKABLE void showPlasmoidMenu(QQuickItem *appletInterface, int x, int y);

    /**
     * Find out global coordinates for a popup given local MouseArea
     * coordinates
     */
    Q_INVOKABLE QPointF popupPosition(QQuickItem *visualParent, int x, int y);

    /**
     * Returns the horizontal panel space between this applet's fixed right
     * edge and the nearest non-spacer panel widget on its left.
     */
    Q_INVOKABLE int availablePanelWidth(QQuickItem *visualParent, int minimumWidth, int gap) const;

    /**
     * @brief isSystemTrayApplet checks if applet is allowed in the System Tray
     * @param appletId also known as plugin Id
     * @return true if it is a system tray applet, otherwise false
     */
    Q_INVOKABLE bool isSystemTrayApplet(const QString &appletId);

    /**
     * Needed to preserve keyboard navigation
     */
    Q_INVOKABLE void stackItemBefore(QQuickItem *newItem, QQuickItem *beforeItem);

    Q_INVOKABLE void stackItemAfter(QQuickItem *newItem, QQuickItem *afterItem);

    Q_INVOKABLE void activate(const QString &service, QPoint pos, QQuickItem *statusNotifierIcon);

    Q_INVOKABLE void secondaryActivate(const QString &service, QPoint pos);

    Q_INVOKABLE void openContextMenu(const QString &service, QPoint pos, QQuickItem *statusNotifierIcon);

    Q_INVOKABLE void scroll(const QString &service, int delta, const QString &direction);
    Q_INVOKABLE void launchApplication(const QString &desktopName);
    Q_INVOKABLE QString resolveApplicationIcon(const QString &applicationName,
                                               const QString &desktopEntry,
                                               const QString &fallbackIcon) const;

    QStringList performancePresets() const;
    QString activePerformancePreset() const;
    Q_INVOKABLE void applyPerformancePreset(const QString &name);
    int volumePercent() const;
    bool volumeMuted() const;
    bool volumeAvailable() const;
    Q_INVOKABLE void setVolumePercent(int percent);

Q_SIGNALS:
    void performancePresetsChanged();
    void activePerformancePresetChanged();
    void volumeChanged();

private Q_SLOTS:
    // synchronizes with configuration and deletes not allowed applets
    void onEnabledAppletsChanged();
    // creates an applet *if not already existing*
    void startApplet(const QString &pluginId);
    // deletes/stops all instances of a given applet
    void stopApplet(const QString &pluginId);

private:
    void migrateFromSystrayContainer();
    SystemTrayModel *systemTrayModel();
    void initSettingsAndRegistry();
    void refreshPerformancePresets();
    void updateDefaultAudioSink(PulseAudioQt::Sink *sink);

    KConfigWatcher::Ptr m_configWatcher;
    bool m_xwaylandClientsScale = true;

    QPointer<SystemTraySettings> m_settings;
    QPointer<PlasmoidRegistry> m_plasmoidRegistry;

    PlasmoidModel *m_plasmoidModel = nullptr;
    StatusNotifierModel *m_statusNotifierModel = nullptr;
    SystemTrayModel *m_systemTrayModel = nullptr;
    SortedSystemTrayModel *m_sortedSystemTrayModel = nullptr;
    SortedSystemTrayModel *m_configSystemTrayModel = nullptr;

    QHash<QString /*plugin id*/, int /*config group*/> m_configGroupIds;
    QStringList m_performancePresets;
    QString m_activePerformancePreset;
    QString m_performanceHelper;
    QPointer<PulseAudioQt::Sink> m_defaultAudioSink;
};
