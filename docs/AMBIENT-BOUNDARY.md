# Temperance boundary with Tettegouche's Ambient

Which status Temperance shows, and which Tettegouche's Ambient shows.

Temperance shows **what changed**. It receives and presents notifications:
the ticker, priority banners, grouped history, dismissal, and the sender's own
actions. It does not turn continuous status into made-up events.

Ambient shows **what matters now**. It shows ongoing context on its own
side of the dock, including media sessions, transfers, their progress, and their
live actions.

Both sides follow these rules:

- The source application or service holds the real state and actions.
- Temperance does not read progress out of notification text, track jobs of its
  own, or keep repeating live media state.
- Ambient removes an activity when its source says it has ended. It does not
  guess completion because the filesystem went quiet or the source disappeared.
- A finished activity's outcome may reach Temperance through the shared
  notification path, but the user sees it only once.
- A transfer's end (`transfer.complete`, `transfer.error`) and a drive plugged
  in (`device.added`) go to the history, not the ticker or a banner: Ambient
  already showed each for a minute.
- Temperance changes for Ambient are limited to a proven handoff, hiding
  duplicates, or shared geometry. Ambient itself is built in Tettegouche.

## Panel geometry

The application dock stays centered on the panel. Temperance measures and
resizes only within its own share of the panel, on whichever side the panel
gives it; Tettegouche has its launcher and Ambient strip on the other. Each side
responds to panel geometry without moving, sizing or configuring the other.

Temperance updates its geometry on events. It does not poll the panel, write
spacer configuration or take the center space. Spare width goes outside whole
activity and control groups, never between the items inside one.
