# Panels

How Temperance fits the panel it sits in, and what a host can tell it.
`ARCHITECTURE.md` holds the rest of how it works.

## Thickness

The row is drawn for a 42 px surface. On a horizontal panel that leaves it under
34 px, such as a 24 px bar along the top of the screen, Temperance draws the slim
row instead. Only the panel's thickness decides; there is no setting for it. A
vertical panel always keeps the full row, and so does a panel 34 px or thicker.

| Part | Full row | Slim row |
| --- | --- | --- |
| Status step | 34 px | 28 px |
| Bell, symbolic and weather icons | 21 px | 16 px |
| Theme status icons, charging bolt | 24 px | 16 px |
| Wired port | 18 px | 14 px |
| Outline line | 1.75 px | 1.5 px |
| Bell's count | 13 px, under the rim | 11 px, on a chip in the bell's place |
| Battery percentage | 15 px | 12 px |
| Weather temperature | 10 px, under the icon | 12 px, beside the icon |
| Tray mark | 12 px | 9 px |
| Ticker line | System font | 13 px |
| Time | 15 px | 13 px |

The slim row has no room under the bell, so with something unread the bell gives
its place, and its touch, to a chip in the highlight colour holding the count: a
16 px circle for one digit, longer for more, 9+ past nine. It counts the lines
waiting and those checked off but not yet read in the history; with nothing
unread the bell comes back, and do not disturb shows the slashed bell and no chip.
The two cross-fade at Plasma's animation speed.
Set to show the bell alone (`waitingShows` set to `bell`), the bar keeps the bell
with its clapper in either row, with no count, ✓ or chip, and the bell's
accessible name says the count instead.

The slim row takes the panel's height and asks for no more. Each control is still
at least one step wide and runs the bar's full height, so a touch anywhere on the
bar lands on a control, and every mark, the count included, stays inside the bar. The
ticker keeps all it does, on one line centred in the bar: notifications, today's
event, a flick to check a line off, a tap to open the history. With the time and
the date both off, as on a bar that draws its own clock, the clock takes no room;
both on, its two lines are tight in a 24 px bar.

A finger cannot choose between controls that small, so in the slim row the
status controls, from the bell to the row's end, take a finger's press as one.
A tap opens the page of the control nearest it, as a click would, a tap between
two taking the nearer, and a second tap on it closes the page.
Pulled 16 px away from the bar, more than a tap's wobble and two thirds of a
24 px bar, the controls grow out of the bar into a strip of their own beside it,
whole at once, however far the finger has gone. There each control is 78 px
from the next, about 16 mm on the Z13, with its mark at 40 px, and the strip is
placed so the control pressed, or the nearer one to a press between two, comes
out under the finger, kept on the screen. That control is chosen first. The
finger then chooses by how far it slides, a control for every 78 px; where the
screen's edge leaves less room than that to one side, the steps that way
shorten so the strip's end is reached just short of the edge. A tile marks
the chosen control and its name shows beneath it. The controls keep still and
keep their size, in the strip and in the bar. Nothing opens while the finger
moves. Lifting over a control opens its page, or leaves it open if it already
was; lifting back in the bar or past either end of the strip opens nothing.
Either way the strip goes back into the bar at Plasma's animation speed. The
strip follows a light or dark style as the pages do. A mouse, touchpad or pen
still uses each control on its own. The bell shows no name over the slim row,
because the page it opens has its own title. The panel gives that run of controls
as `fingerControlsArea`, in its own coordinates, so a bar holding it along the
top of the screen keeps only the controls from a pull meant for the
workspaces and leaves the ticker's room to it.

A desktop that turns off Plasma's own popups can ask the row to show the
volume, the screen's brightness, the keyboard's light and the microphone's
mute instead (`showLevels`, off by default). As one changes, by a key or
another application, the speaker, the sun or the keyboard comes into the row
before Control Center's button with a short track showing how full, or the
microphone alone, struck through while muted, for a second and a half after
the last change, opening and closing at Plasma's animation speed. It stays hidden for the first reading, while
Control Center is open (its sliders already show the level), and when the
setting is off.

## Edges

Pages and important banners open away from the panel's edge, above a panel along
the bottom and below one along the top, and come in from the panel's side. A page
sits 18 px from the row and a banner 10 px further, since its window type has no
clearance of its own. The slim row's strip of status controls sits 4 px from the
bar.

## Host interfaces

A host can publish what Temperance cannot measure. Each interface is optional:
without it, or with an answer Temperance does not read, Temperance behaves as on
any panel.

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
