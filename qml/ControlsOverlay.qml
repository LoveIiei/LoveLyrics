// SPDX-License-Identifier: AGPL-3.0-or-later
// Bottom hover overlay: seek bar, transport (via MPRIS), offset, toggles.
import QtQuick
import LoveLyrics.Backend
import "components"

Item {
    id: root
    required property var theme
    property bool shown: false
    property bool fullScreen: false
    readonly property bool hovered: hover.hovered || seekMouse.pressed
    signal toggleFullScreen()
    signal nextStyle()
    signal offsetChanged()

    height: 150
    opacity: shown ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutQuad } }

    HoverHandler { id: hover }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.55) }
        }
    }

    // Seek bar
    Item {
        id: seek
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: controls.top
        anchors.leftMargin: 32
        anchors.rightMargin: 32
        anchors.bottomMargin: 6
        height: 26

        readonly property real duration: Math.max(1, Clock.durationMs)
        readonly property real fraction: seekMouse.pressed ? seekMouse.dragFraction
                                                           : Math.min(1, Clock.positionMs / duration)

        Text {
            anchors.left: parent.left
            anchors.bottom: track.top
            anchors.bottomMargin: 4
            text: App.formatTime(seek.fraction * seek.duration)
            color: "white"
            opacity: 0.75
            font.pixelSize: 13
            font.features: { "tnum": 1 }
        }
        Text {
            anchors.right: parent.right
            anchors.bottom: track.top
            anchors.bottomMargin: 4
            text: App.formatTime(Clock.durationMs)
            color: "white"
            opacity: 0.75
            font.pixelSize: 13
            font.features: { "tnum": 1 }
        }
        Rectangle {
            id: track
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 4
            height: seekMouse.containsMouse || seekMouse.pressed ? 6 : 4
            radius: height / 2
            color: Qt.rgba(1, 1, 1, 0.22)
            Behavior on height { NumberAnimation { duration: 120 } }
            Rectangle {
                width: parent.width * seek.fraction
                height: parent.height
                radius: parent.radius
                color: "white"
            }
        }
        MouseArea {
            id: seekMouse
            anchors.fill: parent
            anchors.topMargin: 8
            hoverEnabled: true
            enabled: Player.canSeek
            cursorShape: Qt.PointingHandCursor
            property real dragFraction: 0
            function fractionAt(x) { return Math.max(0, Math.min(1, x / width)) }
            onPressed: (e) => dragFraction = fractionAt(e.x)
            onPositionChanged: (e) => { if (pressed) dragFraction = fractionAt(e.x) }
            onReleased: (e) => Player.setPositionMs(Math.round(fractionAt(e.x) * seek.duration))
        }
    }

    Item {
        id: controls
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        height: 56

        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            IconButton {
                label: "−"
                tip: "Lyrics earlier"
                size: 40
                onClicked: { App.adjustTrackOffset(-100); root.offsetChanged() }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: 92
                horizontalAlignment: Text.AlignHCenter
                text: (Clock.trackOffsetMs > 0 ? "+" : "") + (Clock.trackOffsetMs / 1000).toFixed(1) + " s"
                color: "white"
                opacity: Clock.trackOffsetMs !== 0 ? 0.95 : 0.55
                font.pixelSize: 15
                font.features: { "tnum": 1 }
                MouseArea {
                    anchors.fill: parent
                    onDoubleClicked: { App.resetTrackOffset(); root.offsetChanged() }
                }
            }
            IconButton {
                label: "+"
                tip: "Lyrics later"
                size: 40
                onClicked: { App.adjustTrackOffset(100); root.offsetChanged() }
            }
        }

        Row {
            anchors.centerIn: parent
            spacing: 18
            IconButton {
                icon: "prev"
                size: 48
                enabled: Player.canGoPrevious
                onClicked: Player.previous()
            }
            IconButton {
                icon: Player.status === MprisClient.Playing ? "pause" : "play"
                size: 60
                enabled: Player.canControl
                onClicked: Player.playPause()
            }
            IconButton {
                icon: "next"
                size: 48
                enabled: Player.canGoNext
                onClicked: Player.next()
            }
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            IconButton {
                icon: "translate"
                size: 44
                checked: Settings.showTranslation
                visible: Lyrics.hasTranslation
                onClicked: Settings.showTranslation = !Settings.showTranslation
            }
            IconButton {
                icon: "style"
                size: 44
                onClicked: root.nextStyle()
            }
            IconButton {
                icon: root.fullScreen ? "fullscreen-exit" : "fullscreen"
                size: 44
                onClicked: root.toggleFullScreen()
            }
        }
    }
}
