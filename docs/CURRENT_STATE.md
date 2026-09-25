# Current state

Temperance is a native Plasma 6 containment replacing stock System Tray
presentation. Its responsive left rail provides notifications, weather, battery,
Control Center, and an organized tray. Other panel applets remain independent.

The right end carries the time over an optional date, in the system UI font or a
typeface chosen from a searchable list. Digits sit in fixed cells and the clock
reserves the widest time and date its locale produces, so it never moves the
layout. The day period reads a.m. or p.m., lowercase, light and smaller. Each
new minute is dealt onto the old like a card at its own pace, whatever Plasma's
animation speed, and only fades at Instant. The same clock draws a block for a
lock screen at a size the caller gives: a bold time over the long date, its
weekday bold, the date set to exactly the time's width. A tap on the clock opens the month
as a small card in the corner: the current day in the accent, a dot on any day
with a holiday or event, and the chosen day's holidays and events beneath.
Arrows, a sideways swipe or the wheel turn the month. Holidays come from
Plasma's holiday plugin; events from calendars linked by private iCal address in
the Calendar settings, re-read every 10 minutes, keeping the last good copy when
a read fails. Linked calendars are read-only, and each can be given a colour.
The day's next timed event joins the notifications: it reads into the ticker and
holds with a dot in its calendar's colour until the ticker's arrow lands it in
the history, titled Notifications & events, where the day's events head the list
and Dismiss removes one for the day. Physically accepted on 25 September. The
installer needs KDE's calendar library and restarts the panel, so an update
needs no sign-out.

Adaptive width follows the live scene allocation and observes ancestor
AppletContainers. It measures to the nearest applet on its left; on Shuffle's
band, whose dock is painted rather than hosted, the band holds it to its side.
Manual mode uses a fixed configured width. Temperance owns
notification presentation while loaded: normal notices use ticker/history;
critical and selected system alerts may use banners. History preserves producer
actions, defaults, dismissal, grouping, expansion, and application fallback.
External anchors own popup placement; content padding does not.

Checks cover source, notification, session, popup, clock, calendar, and
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

Every popup page sits on one margin line 16 px in from each edge, which the
title, the content's outer edges and the header's last control share; the
spacing, weights and chrome follow the calendar's, physically accepted on 25
September. Section names are quiet 13 px labels, a group's gaps are smaller than
the 24 px between groups, and icons sit 8 px from their labels. Notification
cards have 16 px corners, a soft fill and no outline, 12 px of padding at the
sides and 8 px at top and bottom, and an 8 px gap between cards; titles are 15
px medium, the application and body 13 px in secondary white. Actions keep
small text, 14 px padding, a one-pixel outline and the 44 px input target;
natural popup height remains content-driven and capped by the screen and
card-line allowance. A private NotificationManager integration check proves a
producer-owned `snooze` action is kept in model state and emitted through the
real `ActionInvoked` route. Temperance still does not invent Snooze.
