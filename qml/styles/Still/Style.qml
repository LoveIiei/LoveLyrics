// SPDX-License-Identifier: AGPL-3.0-or-later
// Style 4 — "Still": a calm, readable column. Used automatically for lyrics without
// timestamps; scrolls slowly with the track's progress. Scrolling by hand (wheel/drag)
// pauses the auto-scroll for a few seconds.
import QtQuick
import LoveLyrics.Backend
import "../../components"

Item {
    id: still

    required property var lines
    required property var cursor
    required property real positionMs
    required property var theme
    required property url coverSource
    required property var styleSettings
    property bool showTranslation: true
    property bool showCoverInfo: true
    property bool animate: true

    readonly property real fontSize: Math.round(Math.max(18, Math.min(width * 0.026, height * 0.042))
                                                * (styleSettings.size ?? 1.0) * theme.fontScale)
    readonly property real progress: Math.min(1, Math.max(0, Clock.positionMs / Math.max(1, Clock.durationMs)))

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height + height
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        property bool userScrolled: false
        onMovementStarted: { userScrolled = true; resumeTimer.restart() }
        onFlickStarted: { userScrolled = true; resumeTimer.restart() }
        Timer { id: resumeTimer; interval: 5000; onTriggered: flick.userScrolled = false }

        // The text starts a third of the way down and has scrolled past by the end.
        readonly property real autoY: Math.max(0, still.progress * column.height - still.height * 0.15)
        Binding on contentY {
            when: !flick.userScrolled && !flick.moving
            value: flick.autoY
            restoreMode: Binding.RestoreNone
        }

        WheelHandler {
            onWheel: (e) => {
                flick.userScrolled = true
                resumeTimer.restart()
                flick.contentY = Math.max(0, Math.min(flick.contentHeight - flick.height,
                                                      flick.contentY - e.angleDelta.y))
            }
        }

        Column {
            id: column
            y: still.height * 0.33
            x: (still.width - width) / 2
            width: Math.min(still.width * 0.8, still.fontSize * 32)
            spacing: still.fontSize * 0.5

            Repeater {
                model: still.lines
                delegate: Column {
                    required property string text
                    required property string translation
                    required property bool isInstrumental
                    width: column.width
                    spacing: still.fontSize * 0.15
                    Text {
                        width: parent.width
                        text: parent.isInstrumental ? "♪" : parent.text
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: still.theme.text
                        opacity: 0.88
                        lineHeight: 1.15
                        font.family: still.theme.fontFamily
                        font.weight: Font.Medium
                        font.pixelSize: still.fontSize
                    }
                    Text {
                        width: parent.width
                        visible: still.showTranslation && parent.translation.length > 0
                        text: parent.translation
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        color: still.theme.text
                        opacity: 0.55
                        font.family: still.theme.fontFamily
                        font.pixelSize: Math.round(still.fontSize * 0.7)
                    }
                }
            }
        }
    }

    // Soft fades at the top and bottom edges.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: still.height * 0.2
        gradient: Gradient {
            GradientStop { position: 0.0; color: still.theme.dark }
            GradientStop { position: 1.0; color: Qt.rgba(still.theme.dark.r, still.theme.dark.g, still.theme.dark.b, 0) }
        }
        opacity: 0.85
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: still.height * 0.2
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(still.theme.dark.r, still.theme.dark.g, still.theme.dark.b, 0) }
            GradientStop { position: 1.0; color: still.theme.dark }
        }
        opacity: 0.85
    }

    TrackBadge {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Math.max(24, still.height * 0.04)
        visible: still.showCoverInfo
        theme: still.theme
        coverSource: still.coverSource
        size: Math.max(56, still.height * 0.085)
    }
}
