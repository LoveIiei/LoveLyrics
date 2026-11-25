# Love Lyrics

A native Qt 6 / QML app for KDE Plasma (Wayland) that follows
[Strawberry Music Player](https://www.strawberrymusicplayer.org/) over MPRIS and
shows album art with time‑synced lyrics in immersive, animated full‑screen styles.

It only *displays*: it never touches audio, so Strawberry's bit‑perfect output is
unaffected.

License: AGPL‑3.0‑or‑later. Ideas and some logic are ported from
[Folia](https://github.com/chthollyphile/folia-major); see `NOTICE`.

## Build

Dependencies (Arch / CachyOS):

```fish
sudo pacman -S --needed qt6-base qt6-declarative qt6-shadertools qt6-svg taglib cmake ninja
# for the fake player and integration tests:
sudo pacman -S --needed python-dbus python-gobject
```

```fish
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
./build/lovelyrics
```

Package locally: `cd packaging; makepkg -si`.

## Where lyrics come from

For each track (in this order, configurable):

1. A sidecar file next to the audio: `Song.lrc` (case-insensitive) or `Song.flac.lrc`.
2. Tags embedded in the file: `LYRICS` / `UNSYNCEDLYRICS` (FLAC, Ogg), ID3v2 `SYLT`
   then `USLT` (MP3), `©lyr` (MP4).
3. `xesam:asText` from MPRIS, if the player sends it.

Text files may be UTF‑8, UTF‑16 (with BOM), GB18030/GBK, Big5 or Shift‑JIS.

## Using it

| Key | Action |
|-----|--------|
| `Space` | play / pause |
| `←` / `→` | previous / next track |
| `F`, `F11`, double-click | full screen on/off (`Esc` leaves) |
| `[` / `]` | lyrics 0.1 s earlier / later for this track (saved per track), `\` resets |
| `T` | translations on/off |
| `C` | cover + title corner on/off |
| `S` | next style |
| `,` | settings (`Esc` closes) |
| `D` | debug page |

Click a lyric line to jump to it. Moving the mouse shows the controls; they and the
cursor hide after 2 s.

### Styles

- **Flow** — scrolling lyrics; the active line is large and sharp, the others dim and
  blur with distance, and the list moves like a wave.
- **Kinetic** — one line at a time, very large, in a layout that changes per line; words
  arrive on their timestamps (estimated when the file has none).
- **Stage** — large tilting cover (follows the mouse, drifts when idle) with a
  reflection and progress line; previous / current / next line beside it.
- **Still** — calm centred column. Used automatically for lyrics without timestamps;
  scroll by hand to look around (auto-scroll resumes after 5 s).

Each style has its own tunables in the settings panel (gear button or `,`).

Settings live in `~/.config/lovelyrics/lovelyrics.conf`; the settings panel covers
the player name, where to look for lyrics first, offsets, styles, fonts, background,
the monitor used for full screen and a frame-rate cap (60/120 fps, to save battery on
the 240 Hz panel). `lovelyrics --player NAME`
follows another MPRIS player for one run.

## Testing without Strawberry

`tools/run_with_fake.sh` starts a private D-Bus session with the fake player
(`tools/fake_mpris.py`, playing `tests/data/library`) and the app in it, so a running
Strawberry is never touched and your real settings aren't used (`build/dev/test.conf`):

```fish
tools/run_with_fake.sh
env FAKE_ARGS="--start 40" tools/run_with_fake.sh     # start 40 s in
```

It prints a FIFO path; `echo "s 30" > /tmp/tmp.XXXX` seeks the fake player (`p` play/pause,
`n` next, `release`/`acquire` simulate the player quitting/starting, `jitter 40` adds
noise to position reads; `help` lists all).

Or run the fake player on your normal session bus (only while Strawberry is closed):

```fish
python3 tools/fake_mpris.py
./build/lovelyrics
```

Screenshots for development: `./build/lovelyrics --grab out.png --grab-after 3000`.
Headless (offscreen) runs need `env QT_QUICK_BACKEND=rhi QSG_RHI_BACKEND=opengl
QT_QPA_PLATFORM=offscreen`, otherwise Qt Quick falls back to the software renderer,
which can't draw shaders or blur.

## Hybrid graphics (Intel + NVIDIA)

If the external screen attached to the NVIDIA GPU stutters, start with PRIME
offload:

```fish
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia lovelyrics
# or with Vulkan
env __NV_PRIME_RENDER_OFFLOAD=1 QSG_RHI_BACKEND=vulkan lovelyrics
```

Frame timing: `env QSG_RENDER_TIMING=1 lovelyrics`.

## Full screen on a chosen monitor (Wayland)

The app remembers which screen it was on and goes full screen there. If KWin puts it
on the wrong output anyway, add a window rule: *System Settings → Window Management →
Window Rules → Add New*, match *Window class* `lovelyrics`, then set *Screen* (Force,
the monitor number) and optionally *Fullscreen* (Apply Initially, Yes).
