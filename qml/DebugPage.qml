// SPDX-License-Identifier: AGPL-3.0-or-later
// Developer page: what the backend currently believes. Toggle with D.
import QtQuick
import QtQuick.Layouts
import LoveLyrics.Backend

Rectangle {
    id: page
    color: "#d0101014"

    readonly property var statusNames: ["Stopped", "Paused", "Playing"]

    function fmt(ms) {
        if (ms < 0) return "–"
        const s = Math.floor(ms / 1000)
        const m = Math.floor(s / 60)
        return m + ":" + String(s % 60).padStart(2, "0") + "." + String(Math.floor(ms % 1000)).padStart(3, "0")
    }

    FrameAnimation {
        id: fps
        running: page.visible
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 32
        spacing: 32

        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 360
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 320
                Layout.preferredHeight: 320
                color: Palette.dark
                radius: 8
                clip: true
                Image {
                    anchors.fill: parent
                    source: Cover.source
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                }
                Text {
                    anchors.centerIn: parent
                    visible: !Cover.hasCover
                    text: "no cover"
                    color: "#888"
                }
            }
            Row {
                spacing: 4
                Repeater {
                    model: [Palette.dominant, Palette.vibrant, Palette.muted, Palette.dark, Palette.light, Palette.accent, Palette.text]
                    Rectangle {
                        width: 40
                        height: 40
                        radius: 4
                        color: modelData
                        border.color: "#40ffffff"
                    }
                }
            }
            Image {
                Layout.preferredWidth: 160
                Layout.preferredHeight: 160
                source: Cover.blurredSource
                visible: Cover.hasCover
            }
        }

        GridLayout {
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 20
            rowSpacing: 6

            component Key: Text {
                color: "#9a98a8"
                font.pixelSize: 15
            }
            component Val: Text {
                color: "#f4f2ff"
                font.pixelSize: 15
                font.family: "monospace"
                Layout.fillWidth: true
                elide: Text.ElideMiddle
            }

            Key { text: "Player" }
            Val { text: Player.connected ? (Player.identity || Player.playerName) + " ✓" : "waiting for " + Player.playerName + "…" }
            Key { text: "Status" }
            Val { text: page.statusNames[Player.status] + "  rate " + Player.rate }
            Key { text: "Title" }
            Val { text: Player.title }
            Key { text: "Artist" }
            Val { text: Player.artist }
            Key { text: "Album" }
            Val { text: Player.album }
            Key { text: "URL" }
            Val { text: Player.url }
            Key { text: "Art URL" }
            Val { text: Player.artUrl }
            Key { text: "Position" }
            Val {
                text: page.fmt(Clock.positionMs) + " / " + page.fmt(Clock.durationMs)
                font.pixelSize: 22
            }
            Key { text: "Last correction" }
            Val { text: Clock.lastCorrectionMs.toFixed(1) + " ms" }
            Key { text: "Offset" }
            Val { text: "global " + Clock.globalOffsetMs + " ms, track " + Clock.trackOffsetMs + " ms   ([ / ] to adjust)" }
            Key { text: "Lyrics" }
            Val {
                text: Lyrics.loading ? "loading…" : Lyrics.hasLyrics
                    ? Lyrics.count + " lines, " + (Lyrics.synced ? "synced" : "plain") + (Lyrics.hasWordTiming ? ", word timing" : "")
                      + (Lyrics.hasTranslation ? ", translation" : "") + "  (" + Lyrics.source + ")"
                    : "none"
            }
            Key { text: "Source file" }
            Val { text: Lyrics.sourcePath }
            Key { text: "Cover" }
            Val { text: Cover.hasCover ? Cover.origin + " " + Cover.size.width + "×" + Cover.size.height : "none" }
            Key { text: "Cursor" }
            Val { text: "active " + Cursor.activeIndex + "  focus " + Cursor.focusIndex + "  next " + Cursor.nextIndex + "  progress " + Cursor.progress.toFixed(2) }
            Key { text: "Frame" }
            Val { text: fps.smoothFrameTime > 0 ? (1 / fps.smoothFrameTime).toFixed(0) + " fps" : "–" }

            Item { Layout.preferredHeight: 16; Layout.columnSpan: 2 }

            Text {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 34
                font.weight: Font.Bold
                color: Palette.text
                text: {
                    const i = Cursor.focusIndex + 0 * Lyrics.revision
                    if (i < 0) return "…"
                    const line = Lyrics.get(i)
                    return line.isInstrumental ? "♪" : line.text
                }
                opacity: Cursor.activeIndex >= 0 ? 1 : 0.4
            }
            Text {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 22
                color: Palette.accent
                text: Cursor.focusIndex >= 0 ? Lyrics.get(Cursor.focusIndex).translation : ""
            }
            Rectangle {
                Layout.columnSpan: 2
                Layout.preferredWidth: 400 * Cursor.progress
                Layout.preferredHeight: 3
                color: Palette.accent
            }
        }
    }
}
