#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# Dev helper: run the app against the fake player on a private D-Bus session, so a real
# Strawberry is never involved.  Usage: tools/run_with_fake.sh [lovelyrics args...]
# Fake-player commands can be sent with:  echo "s 40" > "$FIFO"  (path printed at start).
set -eu
here=$(cd "$(dirname "$0")/.." && pwd)
export QT_FORCE_STDERR_LOGGING=1
# Keep dev runs out of the real config.
export LOVELYRICS_CONFIG="${LOVELYRICS_CONFIG:-$here/build/dev/test.conf}"
exec dbus-run-session -- sh -c '
    here="$1"; shift
    fifo=$(mktemp -u); mkfifo "$fifo"; echo "fake player commands: echo CMD > $fifo" >&2
    (tail -f "$fifo" | python3 "$here/tools/fake_mpris.py" --autoplay ${FAKE_ARGS:-} >/dev/null) &
    sleep 0.7
    "$here/build/lovelyrics" "$@"
    status=$?
    kill %1 2>/dev/null; rm -f "$fifo"
    exit $status
' sh "$here" "$@"
