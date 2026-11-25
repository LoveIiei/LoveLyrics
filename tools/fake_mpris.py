#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""A fake MPRIS player for testing Love Lyrics without Strawberry.

    python3 tools/fake_mpris.py tests/data/library/*.lrc "tests/data/library/04 Instrumental.flac"

Each argument is one track. For a .lrc file the track's URL points at a (non-existent)
audio file with the same stem, so sidecar lookup finds the .lrc; [ti:] [ar:] [al:] and
[length:] tags give the metadata. Any other path is used as-is (title from the file name).

Commands (stdin, one per line):  p  play/pause   n  next   b  previous   s SEC  seek to
SEC   +SEC / -SEC  relative seek   r RATE  playback rate   stop   status   quit
release / acquire  drop / re-take the bus name (simulates the player quitting/starting)
jitter MS  make Position answers drift by up to ±MS (tests clock slewing)

Requires python-dbus and python-gobject (no other dependencies).
"""

import argparse
import os
import random
import re
import sys
import time
import urllib.parse

import dbus
import dbus.mainloop.glib
import dbus.service
from gi.repository import GLib

PATH = "/org/mpris/MediaPlayer2"
ROOT = "org.mpris.MediaPlayer2"
PLAYER = "org.mpris.MediaPlayer2.Player"
PROPS = "org.freedesktop.DBus.Properties"


def log(*args):
    print(*args, file=sys.stderr, flush=True)


class Track:
    def __init__(self, index, path, default_length):
        self.index = index
        self.title = os.path.splitext(os.path.basename(path))[0]
        self.artist = "Fake Player"
        self.album = "Fake Album"
        self.length_us = int(default_length * 1_000_000)
        audio = path
        if path.lower().endswith(".lrc"):
            audio = os.path.splitext(path)[0] + ".flac"
            self._read_lrc_tags(path)
        self.path = os.path.abspath(audio)
        self.url = "file://" + urllib.parse.quote(self.path)
        cover = os.path.join(os.path.dirname(self.path), "cover.png")
        self.art_url = ("file://" + urllib.parse.quote(cover)) if os.path.exists(cover) else ""
        self.trackid = "/org/lovelyrics/fake/track%d" % index

    def _read_lrc_tags(self, path):
        last = 0.0
        with open(path, "rb") as f:
            text = f.read().decode("utf-8", "replace")
        for m in re.finditer(r"\[(ti|ar|al|length):([^\]]*)\]", text):
            key, value = m.group(1), m.group(2).strip()
            if key == "ti":
                self.title = value
            elif key == "ar":
                self.artist = value
            elif key == "al":
                self.album = value
            elif key == "length":
                mm, ss = value.split(":")
                self.length_us = int((int(mm) * 60 + float(ss)) * 1_000_000)
        for m in re.finditer(r"\[(\d+):(\d+(?:\.\d+)?)\]", text):
            last = max(last, int(m.group(1)) * 60 + float(m.group(2)))
        if "[length:" not in text and last > 0:
            self.length_us = int((last + 10) * 1_000_000)

    def metadata(self):
        md = {
            "mpris:trackid": dbus.ObjectPath(self.trackid),
            "mpris:length": dbus.Int64(self.length_us),
            "xesam:url": dbus.String(self.url),
            "xesam:title": dbus.String(self.title),
            "xesam:artist": dbus.Array([self.artist], signature="s"),
            "xesam:album": dbus.String(self.album),
        }
        if self.art_url:
            md["mpris:artUrl"] = dbus.String(self.art_url)
        return dbus.Dictionary(md, signature="sv")


class FakePlayer(dbus.service.Object):
    def __init__(self, bus, bus_name, tracks):
        super().__init__(bus, PATH)
        self.bus = bus
        self.bus_name_str = bus_name
        self.name_handle = None
        self.tracks = tracks
        self.index = 0
        self.status = "Stopped"
        self.rate = 1.0
        self.anchor_us = 0
        self.anchor_t = time.monotonic()
        self.jitter_ms = 0
        GLib.timeout_add(100, self._tick)

    # --- bus name -------------------------------------------------------------------
    def acquire(self):
        if self.name_handle is None:
            self.name_handle = dbus.service.BusName(self.bus_name_str, self.bus, do_not_queue=True)
            log("acquired", self.bus_name_str)

    def release(self):
        if self.name_handle is not None:
            self.bus.release_name(self.bus_name_str)
            self.name_handle = None
            log("released", self.bus_name_str)

    # --- position -------------------------------------------------------------------
    @property
    def track(self):
        return self.tracks[self.index]

    def position_us(self):
        pos = self.anchor_us
        if self.status == "Playing":
            pos += int((time.monotonic() - self.anchor_t) * 1_000_000 * self.rate)
        return max(0, min(pos, self.track.length_us))

    def _reanchor(self, pos_us=None):
        self.anchor_us = self.position_us() if pos_us is None else max(0, min(pos_us, self.track.length_us))
        self.anchor_t = time.monotonic()

    def _tick(self):
        if self.status == "Playing" and self.position_us() >= self.track.length_us:
            if self.index + 1 < len(self.tracks):
                self.go(self.index + 1)
            else:
                self.set_status("Stopped")
                self._reanchor(0)
        return True

    # --- state changes ----------------------------------------------------------------
    def changed(self, props):
        self.PropertiesChanged(PLAYER, dbus.Dictionary(props, signature="sv"), dbus.Array([], signature="s"))

    def set_status(self, status):
        if status == self.status:
            return
        self._reanchor()
        self.status = status
        self.changed({"PlaybackStatus": status})
        log("status", status, "at %.2fs" % (self.position_us() / 1e6))

    def go(self, index):
        self.index = index % len(self.tracks)
        self._reanchor(0)
        self.changed({"Metadata": self.track.metadata()})
        log("track", self.index, self.track.title)

    def seek_to(self, pos_us):
        self._reanchor(pos_us)
        self.Seeked(dbus.Int64(self.position_us()))
        log("seeked to %.2fs" % (self.position_us() / 1e6))

    # --- org.mpris.MediaPlayer2 -----------------------------------------------------
    @dbus.service.method(ROOT)
    def Raise(self):
        pass

    @dbus.service.method(ROOT)
    def Quit(self):
        GLib.idle_add(loop.quit)

    # --- org.mpris.MediaPlayer2.Player ----------------------------------------------
    @dbus.service.method(PLAYER)
    def Next(self):
        self.go(self.index + 1)

    @dbus.service.method(PLAYER)
    def Previous(self):
        if self.position_us() > 3_000_000:
            self.seek_to(0)
        else:
            self.go(self.index - 1)

    @dbus.service.method(PLAYER)
    def Pause(self):
        self.set_status("Paused")

    @dbus.service.method(PLAYER)
    def Play(self):
        self.set_status("Playing")

    @dbus.service.method(PLAYER)
    def PlayPause(self):
        self.set_status("Paused" if self.status == "Playing" else "Playing")

    @dbus.service.method(PLAYER)
    def Stop(self):
        self.set_status("Stopped")
        self._reanchor(0)

    @dbus.service.method(PLAYER, in_signature="x")
    def Seek(self, offset_us):
        self.seek_to(self.position_us() + int(offset_us))

    @dbus.service.method(PLAYER, in_signature="ox")
    def SetPosition(self, trackid, pos_us):
        if str(trackid) == self.track.trackid:
            self.seek_to(int(pos_us))

    @dbus.service.method(PLAYER, in_signature="s")
    def OpenUri(self, uri):
        pass

    @dbus.service.signal(PLAYER, signature="x")
    def Seeked(self, position_us):
        pass

    # --- org.freedesktop.DBus.Properties ----------------------------------------------
    def _all(self, interface):
        if interface == ROOT:
            return {
                "CanQuit": True, "CanRaise": False, "HasTrackList": False,
                "Identity": "Fake MPRIS Player", "DesktopEntry": "",
                "SupportedUriSchemes": dbus.Array([], signature="s"),
                "SupportedMimeTypes": dbus.Array([], signature="s"),
            }
        if interface == PLAYER:
            pos = self.position_us()
            if self.jitter_ms:
                pos += random.randint(-self.jitter_ms, self.jitter_ms) * 1000
            return {
                "PlaybackStatus": self.status,
                "LoopStatus": "None",
                "Rate": dbus.Double(self.rate),
                "Shuffle": False,
                "Metadata": self.track.metadata(),
                "Volume": dbus.Double(1.0),
                "Position": dbus.Int64(max(0, pos)),
                "MinimumRate": dbus.Double(0.25),
                "MaximumRate": dbus.Double(4.0),
                "CanGoNext": True, "CanGoPrevious": True, "CanPlay": True,
                "CanPause": True, "CanSeek": True, "CanControl": True,
            }
        raise dbus.exceptions.DBusException("Unknown interface " + interface,
                                            name="org.freedesktop.DBus.Error.UnknownInterface")

    @dbus.service.method(PROPS, in_signature="ss", out_signature="v")
    def Get(self, interface, prop):
        return self._all(interface)[prop]

    @dbus.service.method(PROPS, in_signature="s", out_signature="a{sv}")
    def GetAll(self, interface):
        return self._all(interface)

    @dbus.service.method(PROPS, in_signature="ssv")
    def Set(self, interface, prop, value):
        if interface == PLAYER and prop == "Rate":
            self.set_rate(float(value))

    @dbus.service.signal(PROPS, signature="sa{sv}as")
    def PropertiesChanged(self, interface, changed, invalidated):
        pass

    def set_rate(self, rate):
        self._reanchor()
        self.rate = rate
        self.changed({"Rate": dbus.Double(rate)})


def handle_command(player, line):
    parts = line.strip().split()
    if not parts:
        return
    cmd, args = parts[0], parts[1:]
    try:
        if cmd in ("p", "pause", "play", "toggle"):
            player.PlayPause()
        elif cmd == "n":
            player.Next()
        elif cmd == "b":
            player.Previous()
        elif cmd == "s":
            player.seek_to(int(float(args[0]) * 1_000_000))
        elif cmd[0] in "+-" and len(cmd) > 1:
            player.seek_to(player.position_us() + int(float(cmd) * 1_000_000))
        elif cmd == "r":
            player.set_rate(float(args[0]))
        elif cmd == "stop":
            player.Stop()
        elif cmd == "status":
            log("status=%s track=%d '%s' pos=%.2fs rate=%.2f" % (
                player.status, player.index, player.track.title, player.position_us() / 1e6, player.rate))
        elif cmd == "release":
            player.release()
        elif cmd == "acquire":
            player.acquire()
        elif cmd == "jitter":
            player.jitter_ms = int(args[0])
            log("jitter ±%d ms" % player.jitter_ms)
        elif cmd in ("q", "quit"):
            loop.quit()
        elif cmd == "help":
            log(__doc__)
        else:
            log("unknown command:", line.strip(), "(try 'help')")
    except (IndexError, ValueError) as e:
        log("bad command:", e)


def main():
    global loop
    ap = argparse.ArgumentParser(description="Fake MPRIS player for testing Love Lyrics.")
    ap.add_argument("tracks", nargs="*", help=".lrc files or audio paths")
    ap.add_argument("--bus-name", default="strawberry",
                    help="short player name; registers org.mpris.MediaPlayer2.<name> (default: strawberry)")
    ap.add_argument("--length", type=float, default=180, help="length (s) for non-.lrc tracks")
    ap.add_argument("--autoplay", action="store_true", help="start playing immediately")
    ap.add_argument("--start", type=float, default=0, help="start position (s)")
    opts = ap.parse_args()

    paths = opts.tracks
    if not paths:
        here = os.path.dirname(os.path.abspath(__file__))
        lib = os.path.join(here, "..", "tests", "data", "library")
        paths = sorted(os.path.join(lib, f) for f in os.listdir(lib) if f.endswith(".lrc"))
        paths.append(os.path.join(lib, "04 Instrumental.flac"))
    tracks = [Track(i, p, opts.length) for i, p in enumerate(paths)]

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SessionBus()
    name = "org.mpris.MediaPlayer2." + opts.bus_name
    player = FakePlayer(bus, name, tracks)
    try:
        player.acquire()
    except dbus.exceptions.NameExistsException:
        log("%s is already taken (is the real player running?). Use --bus-name." % name)
        return 1
    if opts.start:
        player._reanchor(int(opts.start * 1_000_000))
    if opts.autoplay:
        player.set_status("Playing")

    def on_stdin(source, condition):
        if condition & GLib.IO_IN:
            line = sys.stdin.readline()
            if line:
                handle_command(player, line)
                return True
        return False  # EOF: keep running, stop watching stdin

    GLib.io_add_watch(sys.stdin, GLib.IO_IN | GLib.IO_HUP, on_stdin)
    log("fake player ready as %s with %d track(s); type 'help'" % (name, len(tracks)))
    print("READY", flush=True)
    loop = GLib.MainLoop()
    try:
        loop.run()
    except KeyboardInterrupt:
        pass
    return 0


loop = None

if __name__ == "__main__":
    sys.exit(main())
