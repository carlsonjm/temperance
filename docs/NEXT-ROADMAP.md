# Next roadmap

This is the sole Temperance execution plan. No implementation packet is active.

## 1. Verify September 17 production candidate

In one physical pass, check notification action padding/hover and a real producer
Snooze action; distinct recent cards and long-history scrolling without clipping;
Control Center and System tray spacing; popup clearance; casing; session glyph
scale; dismissal; and pointer, touch, keyboard, and focus behavior. Accept the
packet or record a measured failure, then freeze passed items.

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
