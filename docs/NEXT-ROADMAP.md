# Next roadmap

This is the sole Temperance execution plan. No implementation packet is active.

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
