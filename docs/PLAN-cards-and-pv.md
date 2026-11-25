# Plan: lyric cards and PV styles

Two features before karaoke (Phase 5), in this order.

## A. Lyric card export

Reference: [spacedge/lyric-card-generator](https://github.com/spacedge/lyric-card-generator)
(AGPL-3.0, cloned to `../ref/lyric-card-generator` for reference). Same licence as ours, so
logic can be ported with attribution (NOTICE + file headers), like Folia.

**How it works here**

- The card is drawn in C++ with `QPainter` (`src/cards/`), porting the reference's Canvas 2D
  templates almost one to one. QPainter gives exact pixels at any size (1080 / 2160 px wide),
  needs no GPU or visible window, and can be unit-tested.
- The editor is a QML panel (like the settings panel). Its preview is the same renderer at
  screen size, served through an image provider and re-rendered on each edit (debounced).
- Opened with `E` or a button in the controls bar; starts with the current line (and its
  neighbours) selected.

**Editor**

| Section | Options |
|---|---|
| Content | pick lines from the current song (checkboxes), mark highlight lines, edit title / artist, translation on/off |
| Template | Compact, Classic, Immersive (blurred cover background, light/medium/heavy), Depth (cover fills the card, sharp top, soft bottom, drag to frame), Whitespace (vertical title, wide margins) |
| Size | long (fits the text), 4:5, 9:16, 1:1 |
| Colour | from the cover: faithful / soft / vivid / mono; light / dark / auto; or pick colours. Text contrast ≥ 7:1 when possible, never below 4.5:1 (reference `color.js`, OKLCH) |
| Text | font (the app's font list), size, alignment |
| Extras | date, small corner text, progress bar (position of the lines in the song) |

Export: PNG 1080 or 2160 px wide → `~/Pictures/Love Lyrics/<artist> - <title>.png`, plus
"Copy to clipboard".

**Port list**: `color.js` (palette, OKLCH, contrast), `text.js` (CJK line breaking: no
line-start punctuation, no orphan characters), `render/templates.js` + `common.js`,
`render/blur.js` (we can reuse our own blur). Not ported: PWA, QR code, furigana (could
come later), font downloads.

**Tests**: palette/contrast rules, line breaking, every template × size renders at
1080/2160 without overflow (text stays inside the card), golden-image smoke check.

## B. PV styles (Folia-inspired)

Folia's PV modes (from `src/components/visualizer/README.md` and the code):

| Folia mode | What it is | Fit for Qt Quick |
|---|---|---|
| 云阶 Partita | line split into rows staggered left/right like steps; each row springs in from its side with an L-shaped guide line drawn at alternating corners; words start scattered, blurred, turned 20°, half size, then spring into place at 1.4× in the accent colour with a glow, then relax and drift | QML only — easy |
| 时计 Pendolo | big clockwork wheel off the left edge; lyric lines are spokes on its right-hand arc; each line change ticks the wheel one spoke with an escapement spring (overshoot + settle); meshing gears, balance wheel, two 60-tick dial rings that light up | QML + Shapes / gear textures — medium |
| 花火 Hanabi | Japanese-PV: words on colour blocks (bands, tickets, highlighter, staircases), fireworks (chrysanthemum, peony, willow, ring, heart…) bursting as lines start, trails rising, the whole world rising and turning between shots; 10 shot families × 5 presets chosen per song | Qt Quick Particles + shaders — hard, biggest wow |
| 绘光 Lumiere | stage lighting: volumetric beams, smoke, floating dust; lyrics lit where beams hit them | shaders — medium |
| 雾笺 Nebbia | foggy bus window in the rain; lyrics are "written" stroke by stroke with a finger, showing the night street outside, then fog over again; droplets | shaders + mask texture — hard |
| 隐喻 Metaphor / Tempera | ink metaballs, halftone, collage typography, hard cuts | later, if wanted |

Folia's "beat FX" use audio analysis (beat tracking, drum separation). We never touch audio,
so effects are driven by lyric timing: line starts, word starts, chorus lines, section
boundaries.

**Shared groundwork (first)**

1. *Song structure*: sections (gaps / instrumental breaks), chorus detection (repeated
   lines), seeded random per song — so every style can pick shots per section and repeat
   identically on every play (port the ideas of Folia's `paragraphStructure.ts` and
   `chorusResolver.ts`).
2. *Effects kit* in QML: spring/escapement helpers, particle bursts (Qt Quick Particles),
   colour blocks, glow (MultiEffect), shape library (stars, rings, lines).
3. *Style quality*: each style has a "quality" tunable and stays within the 4 ms/frame
   budget on the iGPU; heavy layers off when the cap or battery says so.

**Order**: groundwork → 云阶 Partita → 花火 Hanabi → 时计 Pendolo → 绘光 Lumiere → 雾笺 Nebbia.
Each one ends with screenshots, a perf check at 4K, a commit, and a pause for your review.
