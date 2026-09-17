# Itasca visual language

This is the shared presentation reference for Temperance. It guides new work
without replacing app-specific interaction, ownership, or accessibility contracts.

## Intent

Itasca is quiet, spatial, and touch-native. Shape, spacing, contrast, and motion
should explain an object's role before copy does. Preserve established product
silhouettes and source-owned identity.

## Shape and grouping

**Pills are controls.** They act, choose, switch, filter, or change state. Their
radius is half their height. Related controls share alignment and treatment;
icon-only controls may be circular when their target is square. Passive labels
must not look actionable.

**Rounded boxes are information.** Use 14 px corners for compact cards and menus,
16 px for list/result cards, and 18 px for substantial popup or alert surfaces.
Information boxes may contain pills, but the outer surface remains visually
distinct.

Preserve rounded continuity:

- Align adjacent control centerlines and use one radius treatment per family.
- Keep at least 4 px between separate pills; use 6–8 px outside a tight cluster.
- Make highlights follow the object's silhouette.
- Put surplus width after a complete group, never between its children.

## Spacing

Use a 4 px base rhythm.

| Role | Value | Use |
|---|---:|---|
| Hairline | 2 px | Optical correction or stacked-label gap |
| Tight | 4 px | One cluster or icon/badge relation |
| Compact | 8 px | Icon-to-label and compact grouping |
| Standard | 12 px | Card padding and ordinary rows |
| Comfortable | 16 px | Larger control inset |
| Section | 20 px | Pane columns and major local groups |
| Surface | 24 px | Popup breathing room and section separation |

Spacing inside a group stays smaller than spacing between groups. Compact glyphs
do not reduce touch targets; panel controls normally retain 40–44 px targets.
Responsive layouts remove optional information before crushing useful spacing and
use available space before eliding content.

## Color

| Role | Value | Purpose |
|---|---|---|
| Surface | `#141414` | Primary popup/card surface |
| Raised control | `#242424` | Resting filled control |
| Divider / quiet border | `#333333` | Low-emphasis separation |
| Strong border | `#5A5A5A` | Focusable edge |
| Primary / Ghost White | `#F8F8FF` | Main labels and suite glyphs |
| Soft primary | `#F2FFFFFF` | Large surfaces where full white is too bright |
| Secondary | `#A8FFFFFF` | Supporting labels and metadata |
| Edge hint | `#88FFFFFF` | Quiet spatial affordance |
| Subtle fill | 7–8% white | Resting information separation |
| Hover fill | 12–13% white | Pointer hover |
| Selected fill | 24% white | Current local selection |
| Accent | Plasma/user accent | Active state, progress, high-value signal |
| Accent foreground | `#102729` | Content on a bright accent |
| Error | `#FFB5A8` | Actionable failure |

Accent communicates state rather than decoration. Prefer opacity steps of soft
white to unrelated grays. Application artwork and provider identity may keep native
color; suite chrome stays monochrome unless state requires accent or error color.

## Interaction states

| State | Treatment |
|---|---|
| Resting | Transparent or `#242424` when the boundary must remain visible |
| Hover | 12–13% white fill, 100–150 ms ease-out |
| Pressed | Slightly stronger fill without dramatic scale |
| Selected | 24% white fill or accent for a durable active state |
| Keyboard focus | Near-white/strong outline on the same silhouette |
| Disabled | Reduced contrast without implying selection |
| Attention | Small accent, badge, or purposeful motion |
| Error | Warm error copy/icon with a direct recovery action |

An isolated panel icon may scale to about 1.06 on hover. Do not combine scale,
strong fill, outline, and color change for one ordinary hover.

## Motion

Motion explains state, causality, continuity, or authoritative completion.

| Tier | Duration | Use |
|---|---:|---|
| Immediate | 80–120 ms | Press and tiny acknowledgement |
| Micro | 120–160 ms | Hover, focus, toggle, icon state |
| Local | 180–240 ms | Expand/collapse or content replacement |
| Spatial | 240–360 ms | Popup, card entrance, edge travel |
| Completion hold | 700–900 ms | Brief authoritative success before removal |

Use cubic ease-out for entrances and ease-in-out for reversible state changes.
Avoid bounce or overshoot for factual state. Animate a container before decorating
its children; keep control clusters together; keep stagger below about 60 ms.

Every transition must be interruptible, reversible, or retargetable from its
visible state. User input wins immediately. The state owner owns completion:
presentation may interpolate known state but must not invent progress, success, or
failure. Source loss removes stale activity without a false success animation, and
stable identity updates an existing surface instead of replaying its entrance.

Follow the platform animation scale. Under reduced motion, keep final geometry and
state communication with short opacity or color changes; remove travel, overshoot,
and stagger. Animation is never the only state signal.

## Type and casing

Use the system UI family and create hierarchy with size, weight, opacity, and
spacing.

| Role | Treatment |
|---|---|
| Primary content | 15–16 px, regular/medium, primary color |
| Section/card title | 15–16 px, demi-bold only when useful |
| Supporting metadata | 11–13 px, secondary color |
| Compact panel content | Established panel size and restrained weight |
| Large transition label | 20–24 px, demi-bold, short text |
| Tiny state tag | 10–11 px, bold, lightly tracked, rare |

Use sentence case for headings, menus, buttons, settings, and descriptions. Use
lowercase for quiet environmental invitations such as `browse everything`. Use
all caps only for a tiny established state tag. Preserve source casing for people,
applications, files, songs, and artists; title case is for proper names.

## Icons

Lucide Static 1.46.0 is the canonical family for suite-owned action chrome.
Repositories vendor only the pinned SVG subset they consume and retain the
ISC/Feather MIT notice.

- Use rounded, optically balanced geometry on a consistent grid.
- Use monochrome/current-color SVGs for suite chrome.
- Test size and stroke at actual panel scale.
- Do not ship Unicode symbols as control icons or mix families in one cluster.
- Keep semantic asset names consistent across repositories.
- Keep KDE/provider lookup for applications, files, weather, tray entries, and
  other externally owned identity.

Temperance's animated bell uses Lucide Bell geometry but separates its clapper and
slash for behavior. The Tette Dot, Temperance bell, Temperance weather treatment,
and custom tray work are protected product components. Do not replace or redraw
them as part of a general Lucide migration.

## Responsive composition

Width reveals information, not capability:

1. Measure the real available span.
2. Reserve the actionable core and its spacing.
3. Add context in product-priority order at natural widths.
4. Use exact remaining width before eliding.
5. Remove optional information only below its useful minimum.
6. Keep complete activities or control groups together and place unused width
   afterward.

Ambient media is the reference composition:

`[previous · play/pause · next]  Song  Artist  runtime`

## Review

Check that shape distinguishes controls from information; grouping and spacing
show relationships; available space is used before truncation; hover, focus,
pressed, and selected states differ; copy follows its casing role; color comes from
semantic roles; panel and popup layouts survive target widths; icons share an
approved family and optical scale; and motion remains truthful and interruptible.

Soft primary and app-specific outer silhouettes remain deliberate local choices.
Changing either across the suite requires its own roadmap item.
