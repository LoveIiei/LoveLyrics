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

## Testing without Strawberry

`tools/fake_mpris.py` registers a fake player on the session bus:

```fish
python3 tools/fake_mpris.py            # plays tests/data/library
# then in another terminal
./build/lovelyrics
```

Type `help` in the fake player for commands (play/pause, seek, next, …).
Use `--bus-name strawberry` (the default) to impersonate Strawberry, or another
name together with `lovelyrics --player <name>`.

## Hybrid graphics (Intel + NVIDIA)

If the external screen attached to the NVIDIA GPU stutters, start with PRIME
offload:

```fish
env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia lovelyrics
# or with Vulkan
env __NV_PRIME_RENDER_OFFLOAD=1 QSG_RHI_BACKEND=vulkan lovelyrics
```

Frame timing: `env QSG_RENDER_TIMING=1 lovelyrics`.
