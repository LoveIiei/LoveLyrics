# Styles

Each style is one QML file, `qml/styles/<Name>/Style.qml`, listed in `registry.js`.
`Main.qml` loads it with these properties:

| Property          | Type          | What it is                                                    |
|-------------------|---------------|---------------------------------------------------------------|
| `lines`           | `LyricsModel` | rows: `startMs endMs text translation romanization words isInstrumental wordsEstimated`; also `synced`, `hasWordTiming`, `get(i)` |
| `cursor`          | `LyricsCursor`| `activeIndex`, `focusIndex`, `nextIndex`, `progress`, `timeToNextMs` |
| `positionMs`      | `real`        | lyrics position (user offsets applied), updated once per frame |
| `theme`           | `Theme`       | animated palette (`dark text accent vibrant muted light dominant`) and font settings (`fontFamily fontWeight fontScale`) |
| `coverSource`     | `url`         | current cover (`image://cover/...`), empty without cover      |
| `styleSettings`   | `object`      | per-style tunables (defaults in `registry.js`, saved values from settings) |
| `showTranslation` | `bool`        | `T` toggles                                                   |
| `showCoverInfo`   | `bool`        | `C` toggles                                                   |
| `animate`         | `bool`        | false while the window is minimised: stop idle motion         |

The plan called the theme property `palette`; it is `theme` because every Qt Quick
`Item` already has a `palette` property.

Rules of thumb:

- Animate `x/y/scale/opacity` (or a `Translate`), never `font.pixelSize`.
- Use `layer.enabled` / `MultiEffect` only on items that need it right now.
- Don't name an `id` `style` — `Text.style` shadows it inside `Text` elements.
- Don't use `visible: false` to cull children of a `Column`: positioners skip invisible
  items. Hide an inner item instead.
