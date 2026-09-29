# Third-party icon assets

The suite-owned action glyphs in `lucide/` are a pinned subset of Lucide
`lucide-static` version 1.46.0. Lucide is licensed under ISC; inherited Feather
icons retain their MIT notice. The complete upstream notice is in
`lucide/LICENSE`. `qml/BellGlyph.qml` is the documented Lucide-derived exception:
it separates bell parts to animate the clapper and notification slash. Provider,
tray, application, weather, and other external identity icons remain source-owned.

Vendored SVG roots set `color="#F8F8FF"` (Ghost White) so Qt resolves the upstream `currentColor` strokes consistently; path geometry is unchanged.
