# Panels

How Temperance fits the panel it sits in, and what a host can tell it.
`ARCHITECTURE.md` holds the rest of how it works.

## Thickness

The row is drawn for a 42 px surface. On a horizontal panel that leaves it under
34 px, such as a 24 px bar along the top of the screen, Temperance draws the slim
row instead. The panel's thickness alone decides; no setting picks it, a vertical
panel always keeps the full row, and at 34 px or more nothing changes.

| Part | Full row | Slim row |
| --- | --- | --- |
| Status step | 34 px | 28 px |
| Bell, symbolic and weather icons | 21 px | 16 px |
| Theme status icons, charging bolt | 24 px | 16 px |
| Wired port | 18 px | 14 px |
| Outline line | 1.75 px | 1.5 px |
| Bell's count | 13 px, under the rim | 10 px, at the crown beside it |
| Battery percentage | 15 px | 12 px |
| Weather temperature | 10 px, under the icon | 12 px, beside the icon |
| Tray mark | 12 px | 9 px |
| Ticker line | System font | 13 px |
| Time | 15 px | 13 px |

The slim row takes the panel's height and asks for no more. Each control is still
at least one step wide and runs the bar's full height, so a touch anywhere on the
bar is a control's, and every mark, the count included, stays inside the bar. The
ticker keeps all it does, on one line centred in the bar: notifications, today's
event, a flick to check a line off, a tap to open the history. With the time and
the date both off, as on a bar that draws its own clock, the clock takes no room;
both on, its two lines are tight in a 24 px bar.

## Edges

Pages and important banners open away from the panel's edge, above a panel along
the bottom and below one along the top, and come in from the panel's side. A page
sits 18 px from the row and a banner 10 px further, since its window type has no
clearance of its own.

## Host interfaces

A host can publish what Temperance cannot measure. Each interface is optional:
without it, or with an answer Temperance does not read, it behaves as on any
panel.

### Clock extent

A bar that paints its own clock, rather than holding a clock applet, says how much
room it leaves beside it, so adaptive width has a neighbour to measure to.

| Part | Value |
| --- | --- |
| Bus | Session |
| Service | `co.goodinput.ContextBar` |
| Path | `/ContextBar` |
| Interface | `co.goodinput.ContextBar` |
| Method | `clockExtent(s outputName) → s`, compact UTF-8 JSON |
| Signal | `clockExtentChanged(s outputName)` |

```json
{"schema":"co.goodinput.shuffle.clock-extent","version":1,"output":"eDP-1","presenting":true,"bar":{"height":24},"clock":{"left":1240,"right":1292,"center":1266},"available":{"left":1224,"right":612}}
```

Coordinates are the output's logical pixels, x from its left edge. `available.right`
is the width from the clock's right edge, with the bar's 16 px gap, to the
output's right edge. An empty output name means the first bar.

Temperance asks once, without waiting on the answer, when it starts, naming the
output its window is on. It asks again only when the signal names that output or
none, when its window moves to another output, or when the service appears; it
never polls. A reply with this schema, version 1 and `presenting` true makes
`available.right` the adaptive width, with the row's own right inset inside it,
and neighbours are not measured. Another version, a bar not presenting, an error
or no service leaves the width to the neighbour measurement in `ARCHITECTURE.md`
§ Width, and the service leaving hands it back there. A fixed width stays fixed.
