# Third-party icon assets

The action glyphs in `lucide/` are a pinned subset of Lucide
`lucide-static` version 1.46.0. Lucide is licensed under ISC; inherited Feather
icons retain their MIT notice. The complete upstream notice is in
`lucide/LICENSE`. `qml/BellGlyph.qml` is the documented Lucide-derived exception:
it separates bell parts to animate the clapper and notification slash. Provider,
application, weather and other external identity icons come from their own
sources.

Vendored SVG roots set `color="#F8F8FF"` (Ghost White) so Qt resolves the upstream `currentColor` strokes consistently; path geometry is unchanged.

# Colloid status icons

The status icons in `colloid/` (network, volume, microphone, Bluetooth, battery
and the other Plasma status widgets Control Center shows) are copied from the
Colloid icon theme, https://github.com/vinceliuice/Colloid-icon-theme, at commit
`ceac6608ecd0e40025cbc2ebbd32bf0e0f4ebc6a`: its bold panel set, and its regular
set where the bold one has no drawing. `tools/status-glyphs.py` copies them and
paints Colloid's `#363636` Ghost White; geometry is unchanged. Colloid is
licensed under the GNU General Public License version 3, in
`../../LICENSES/GPL-3.0-only.txt`.
