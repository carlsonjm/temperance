# Current state

Temperance is a native Plasma 6 containment replacing stock System Tray
presentation. Its responsive left rail provides notifications, weather, battery,
Control Center, and an organized tray. Other panel applets remain independent.

Adaptive width follows the live scene allocation and observes ancestor
AppletContainers. It measures to the nearest applet on its left, and where a
container publishes furniture it painted rather than hosted, it measures to
that published edge instead: such furniture is absent from the applet list, so
measuring to the nearest applet measures straight past it. The published path
is discovered at run time, refuses a payload whose major version it does not
know, and is simply absent on a plain panel, where every measurement is
unchanged. Manual mode uses a fixed configured width. Temperance owns
notification presentation while loaded: normal notices use ticker/history;
critical and selected system alerts may use banners. History preserves producer
actions, defaults, dismissal, grouping, expansion, and application fallback.
External anchors own popup placement; content padding does not.

Checks cover source, notification, session, popup, published-extent, and
real-panel geometry contracts. The real-panel geometry fixture pairs Temperance
with a separately built Tettegouche plugin and is therefore not part of the
automatic run. The release target is x86_64 Plasma 6.7.4, Qt 6.11.2, and KDE
Frameworks 6.29 on a horizontal Wayland panel. Rebuild after incompatible native
upgrades.

Known limitations:

- Stock/Temperance presenter switching is not implemented.
- There is no general system-event feed beyond notification sources.
- Online weather fallback shares configured location data with Open-Meteo.
- The September 17 presentation polish is physically accepted.

The accepted September 17 production state aligns notification actions with the kit:
small text, 14 px horizontal padding, a one-pixel outline, hover fill, and the
existing 44 px input target. Recent cards have a quiet outline and reserve their
full action-row height plus an 8 px inter-card gap; natural popup height remains
content-driven and capped by the existing screen/card-line allowance. A private
NotificationManager integration check proves a producer-owned `snooze` action is
kept in model state and emitted through the real `ActionInvoked` route. Temperance
still does not invent Snooze. Control Center and organized tray geometry now name
the accepted 8/12/24 px spacing tokens without changing their hierarchy.
