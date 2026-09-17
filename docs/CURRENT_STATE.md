# Current state

Temperance is a native Plasma 6 containment replacing stock System Tray
presentation. Its responsive left rail provides notifications, weather, battery,
Control Center, and an organized tray. Other panel applets remain independent.

Adaptive width follows the live scene allocation and observes ancestor
AppletContainers. Manual mode uses a fixed configured width. Temperance owns
notification presentation while loaded: normal notices use ticker/history;
critical and selected system alerts may use banners. History preserves producer
actions, defaults, dismissal, grouping, expansion, and application fallback.
External anchors own popup placement; content padding does not.

Checks cover source, notification, session, popup, and real-panel geometry
contracts. The release target is x86_64 Plasma 6.7.4, Qt 6.11.2, and KDE
Frameworks 6.29 on a horizontal Wayland panel. Rebuild after incompatible native
upgrades.

Known limitations:

- Stock/Temperance presenter switching is not implemented.
- There is no general system-event feed beyond notification sources.
- Online weather fallback shares configured location data with Open-Meteo.
- The latest presentation polish awaits one physical verification pass.
