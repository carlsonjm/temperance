# Next roadmap

This is the sole Temperance execution plan. Order across the suite is Kadunce's
`docs/ROADMAP-CC.md`, where the clock is Block 5's next task.

## 1. September 17 presentation polish

**Status:** Complete and physically accepted.

Notification action padding and hover, producer-owned Snooze dispatch, distinct
recent cards, popup fitting, and Control Center/System Tray spacing passed the
September 17 physical review. Reopen only for a measured regression.

## 2. Decide on presenter switching

If still required, prove the process-wide notification lifecycle first. Keep
Temperance active until stock presentation is ready, avoid duplicates, fall back
on failure, and never change D-Bus ownership or clear shared history.

## 3. Consider event sources

For a small authoritative source set, define identity, freshness, priority,
deduplication, dismissal, and accessibility. Temperance may report transitions;
Ambient Tette retains live progress and actions. Polling is not an event.

Packaging or broad visual work requires a bounded entry with validation and
rollback criteria. Evidence belongs in `archive/`.

## 4. Clock and calendar

**Status:** Slices 1 to 4 passed on the tablet on 25 September; the next event
beside the time (5) is next. Direction settled (`DECISIONS.md` § The clock is Temperance's, and
its minute deals like a card); J approves each slice on the tablet.

1. Time and date at the Status Bar's right end: the system UI font or a
   typeface the person picks, digits of fixed width, 12 or 24 hours from the
   locale, the day period lowercase, light and smaller. The README stops
   calling the clock an independent applet.
2. The new minute deals onto the old like a card, within the state-transition
   tier at its own pace whatever Plasma's animation speed, interruptible, and a
   fade under reduced motion.
3. A tap opens the calendar as a small card in the corner, built on Plasma's
   month grid (`org.kde.plasma.workspace.calendar`) and its holiday plugin,
   both installed on the tablet.
4. Events from a pasted private calendar link (`DECISIONS.md`), read with
   KCalendarCore, which is a small KF6 package not yet on the tablet and so
   becomes a build and install dependency.
5. Bring J the next event beside the time where there is room.
