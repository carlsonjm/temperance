/*
    SPDX-FileCopyrightText: 2020 Konrad Materka <materka@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "systemtraysettings.h"

#include "debug.h"

#include <KConfigLoader>

static const QString KNOWN_ITEMS_KEY = QStringLiteral("knownItems");
static const QString EXTRA_ITEMS_KEY = QStringLiteral("extraItems");
static const QString SHOW_ALL_ITEMS_KEY = QStringLiteral("showAllItems");
static const QString SHOWN_ITEMS_KEY = QStringLiteral("shownItems");
static const QString HIDDEN_ITEMS_KEY = QStringLiteral("hiddenItems");
static const QString DISABLED_SNIS_KEY = QStringLiteral("disabledStatusNotifiers");
static const QString CONTROL_CENTER_ITEMS_KEY = QStringLiteral("controlCenterItems");

SystemTraySettings::SystemTraySettings(KConfigLoader *config, QObject *parent)
    : QObject(parent)
    , config(config)
{
    connect(config, &KConfigLoader::configChanged, this, [this]() {
        if (!updatingConfigValue) {
            loadConfig();
        }
    });

    loadConfig();
}

bool SystemTraySettings::isKnownPlugin(const QString &pluginId)
{
    return m_knownItems.contains(pluginId);
}

const QStringList SystemTraySettings::knownPlugins() const
{
    return m_knownItems;
}

void SystemTraySettings::addKnownPlugin(const QString &pluginId)
{
    m_knownItems << pluginId;
    writeConfigValue(KNOWN_ITEMS_KEY, m_knownItems);
}

void SystemTraySettings::removeKnownPlugin(const QString &pluginId)
{
    m_knownItems.removeAll(pluginId);
    writeConfigValue(KNOWN_ITEMS_KEY, m_knownItems);
}

// A plasmoid promoted to a Control Center pill is loaded too: the pill draws
// only a loaded applet, and an applet off by default, such as KDE Connect's,
// is otherwise never loaded on a fresh Status.
bool SystemTraySettings::isEnabledPlugin(const QString &pluginId) const
{
    return m_extraItems.contains(pluginId) || m_controlCenterItems.contains(pluginId);
}

const QStringList SystemTraySettings::enabledPlugins() const
{
    return m_extraItems;
}

void SystemTraySettings::addEnabledPlugin(const QString &pluginId)
{
    const bool was = isEnabledPlugin(pluginId);
    m_extraItems << pluginId;
    writeConfigValue(EXTRA_ITEMS_KEY, m_extraItems);
    if (!was) {
        Q_EMIT enabledPluginsChanged({pluginId}, {});
    }
}

void SystemTraySettings::removeEnabledPlugin(const QString &pluginId)
{
    m_extraItems.removeAll(pluginId);
    writeConfigValue(EXTRA_ITEMS_KEY, m_extraItems);
    if (!isEnabledPlugin(pluginId)) {
        Q_EMIT enabledPluginsChanged({}, {pluginId});
    }
}

bool SystemTraySettings::isDisabledStatusNotifier(const QString &pluginId)
{
    return m_disabledStatusNotifiers.contains(pluginId);
}

bool SystemTraySettings::isShowAllItems() const
{
    return config->property(SHOW_ALL_ITEMS_KEY).toBool();
}

const QStringList SystemTraySettings::shownItems() const
{
    return config->property(SHOWN_ITEMS_KEY).toStringList();
}

const QStringList SystemTraySettings::hiddenItems() const
{
    return config->property(HIDDEN_ITEMS_KEY).toStringList();
}

const QStringList SystemTraySettings::controlCenterItems() const
{
    return config->property(CONTROL_CENTER_ITEMS_KEY).toStringList();
}

void SystemTraySettings::cleanupPlugin(const QString &pluginId)
{
    removeKnownPlugin(pluginId);
    removeEnabledPlugin(pluginId);

    QStringList shown = shownItems();
    shown.removeAll(pluginId);
    writeConfigValue(SHOWN_ITEMS_KEY, shown);

    QStringList hidden = hiddenItems();
    hidden.removeAll(pluginId);
    writeConfigValue(HIDDEN_ITEMS_KEY, hidden);
}

void SystemTraySettings::loadConfig()
{
    if (!config) {
        return;
    }
    config->load();

    m_knownItems = config->property(KNOWN_ITEMS_KEY).toStringList();

    const QStringList extraItems = config->property(EXTRA_ITEMS_KEY).toStringList();
    const QStringList controlCenter = config->property(CONTROL_CENTER_ITEMS_KEY).toStringList();
    if (extraItems != m_extraItems || controlCenter != m_controlCenterItems) {
        const QStringList enabledOld = m_extraItems + m_controlCenterItems;
        m_extraItems = extraItems;
        m_controlCenterItems = controlCenter;
        notifyAboutChangedEnabledPlugins(enabledOld, m_extraItems + m_controlCenterItems);
    }
    m_disabledStatusNotifiers = config->property(DISABLED_SNIS_KEY).toStringList();

    Q_EMIT configurationChanged();
}

void SystemTraySettings::writeConfigValue(const QString &key, const QVariant &value)
{
    if (!config) {
        return;
    }

    KConfigSkeletonItem *item = config->findItemByName(key);
    if (item) {
        updatingConfigValue = true;
        item->setWriteFlags(KConfigBase::Notify);
        item->setProperty(value);
        config->save();
        // refresh state of config scheme, if not, above writes are ignored
        config->read();
        updatingConfigValue = false;
    }

    Q_EMIT configurationChanged();
}

void SystemTraySettings::notifyAboutChangedEnabledPlugins(const QStringList &enabledPluginsOld, const QStringList &enabledPluginsNew)
{
    QStringList newlyEnabled;
    QStringList newlyDisabled;

    for (const QString &pluginId : enabledPluginsOld) {
        if (!enabledPluginsNew.contains(pluginId)) {
            newlyDisabled << pluginId;
        }
    }

    for (const QString &pluginId : enabledPluginsNew) {
        if (!enabledPluginsOld.contains(pluginId)) {
            newlyEnabled << pluginId;
        }
    }

    Q_EMIT enabledPluginsChanged(newlyEnabled, newlyDisabled);
}

#include "moc_systemtraysettings.cpp"
