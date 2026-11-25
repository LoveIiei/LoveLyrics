// SPDX-License-Identifier: AGPL-3.0-or-later
// Style 3 — "Stage": the cover is the star. Large cover on the left with a soft shadow,
// a slow 3D tilt (follows the mouse, drifts on its own when idle), a reflection and a
// progress line; a three-line lyric window (previous / current / next) on the right.
import QtQuick
import QtQuick.Effects
import LoveLyrics.Backend
import "../../components"

Item {
    id: stage

    required property var lines
    required property var cursor
    required property real positionMs
    required property var theme
    required property url coverSource
    required property var styleSettings
    property bool showTranslation: true
    property bool showCoverInfo: true
    property bool animate: true

    readonly property real tiltAmount: styleSettings.tilt ?? 1.0
    readonly property bool reflection: styleSettings.reflection ?? true

    readonly property bool wide: width > height * 1.15
    readonly property real coverSize: wide ? Math.min(height * 0.56, width * 0.36) : Math.min(width * 0.5, height * 0.34)
    readonly property real fontSize: Math.round(Math.max(22, Math.min(width * 0.032, height * 0.058)) * theme.fontScale)

    // ---- tilt: mouse when it moved recently, else a slow idle drift -------------------
    HoverHandler { id: hover }
    property real mouseX: 0
    property real mouseY: 0
    property bool mouseRecent: false
    Connections {
        target: hover
        function onPointChanged() {
            stage.mouseX = (hover.point.position.x / Math.max(1, stage.width) - 0.5) * 2
            stage.mouseY = (hover.point.position.y / Math.max(1, stage.height) - 0.5) * 2
            stage.mouseRecent = true
            mouseTimer.restart()
        }
    }
    Timer { id: mouseTimer; interval: 4000; onTriggered: stage.mouseRecent = false }

    property real idlePhase: 0
    NumberAnimation on idlePhase {
        from: 0; to: Math.PI * 2; duration: 24000
        loops: Animation.Infinite
        running: stage.animate && Clock.playing
    }
    readonly property real targetTiltY: (mouseRecent ? mouseX * 14 : Math.sin(idlePhase) * 7) * tiltAmount
    readonly property real targetTiltX: (mouseRecent ? -mouseY * 10 : Math.cos(idlePhase * 2) * 3) * tiltAmount
    property real tiltY: targetTiltY
    property real tiltX: targetTiltX
    Behavior on tiltY { SmoothedAnimation { velocity: 20 } }
    Behavior on tiltX { SmoothedAnimation { velocity: 20 } }

    // ---- cover ---------------------------------------------------------------------------
    Item {
        id: coverArea
        width: stage.coverSize
        height: stage.coverSize
        x: stage.wide ? stage.width * 0.08 : (stage.width - width) / 2
        y: stage.wide ? (stage.height - height) / 2 - stage.height * 0.04 : stage.height * 0.1

        transform: [
            Rotation {
                origin.x: coverArea.width / 2
                origin.y: coverArea.height / 2
                axis { x: 0; y: 1; z: 0 }
                angle: stage.tiltY
            },
            Rotation {
                origin.x: coverArea.width / 2
                origin.y: coverArea.height / 2
                axis { x: 1; y: 0; z: 0 }
                angle: stage.tiltX
            }
        ]

        RoundedImage {
            id: cover
            anchors.fill: parent
            radius: width * 0.035
            source: stage.coverSource
            visible: Cover.hasCover
        }
        Rectangle { // placeholder without a cover
            anchors.fill: parent
            radius: width * 0.035
            visible: !Cover.hasCover
            color: Qt.rgba(1, 1, 1, 0.06)
            border.color: Qt.rgba(1, 1, 1, 0.12)
            Text {
                anchors.centerIn: parent
                text: "♪"
                color: stage.theme.text
                opacity: 0.4
                font.pixelSize: Math.round(parent.height * 0.3)
            }
        }

        // Progress along the bottom edge of the cover.
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.bottom
            anchors.topMargin: parent.height * 0.035
            height: Math.max(3, parent.height * 0.008)
            radius: height / 2
            color: Qt.rgba(1, 1, 1, 0.18)
            Rectangle {
                width: parent.width * Math.min(1, Clock.positionMs / Math.max(1, Clock.durationMs))
                height: parent.height
                radius: parent.radius
                color: stage.theme.accent
            }
        }

        // Reflection: a flipped live copy of the cover, faded out downwards.
        Item {
            id: reflectionBox
            visible: stage.reflection && Cover.hasCover
            anchors.top: parent.bottom
            anchors.topMargin: parent.height * 0.07
            width: parent.width
            height: parent.height * 0.4
            clip: true
            opacity: 0.25
            layer.enabled: visible
            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: reflectionMask
                maskThresholdMin: 0.0
                maskSpreadAtMin: 1.0
            }
            ShaderEffectSource {
                width: cover.width
                height: cover.height
                sourceItem: cover
                live: true
                transform: Scale { yScale: -1; origin.y: cover.height / 2 }
            }
        }
        Rectangle {
            id: reflectionMask
            width: reflectionBox.width
            height: reflectionBox.height
            visible: false
            layer.enabled: true
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 1.0) }
                GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0.0) }
            }
        }
    }

    // ---- lyrics: previous / current / next ------------------------------------------------
    readonly property int centerIndex: cursor.focusIndex
    readonly property bool synced: lines.synced

    // Slide the window up when the line changes.
    property real slide: 0
    onCenterIndexChanged: {
        slideAnim.stop()
        slide = 1
        slideAnim.start()
    }
    NumberAnimation {
        id: slideAnim
        target: stage
        property: "slide"
        to: 0
        duration: 620
        easing.type: Easing.OutCubic
    }

    function lineText(i) {
        if (i < 0 || i >= lines.count)
            return ""
        const l = lines.get(i)
        return l.isInstrumental ? "♪" : l.text
    }

    Column {
        id: lyricWindow
        x: stage.wide ? coverArea.x + coverArea.width + stage.width * 0.07 : stage.width * 0.08
        width: stage.wide ? stage.width - x - stage.width * 0.07 : stage.width * 0.84
        y: stage.wide ? (stage.height - height) / 2 : coverArea.y + coverArea.height + stage.height * 0.12
        spacing: stage.fontSize * 0.7
        transform: Translate { y: stage.slide * stage.fontSize * 1.6 }

        Column {
            width: parent.width
            spacing: stage.fontSize * 0.2
            Text {
                width: parent.width
                text: Player.title
                elide: Text.ElideRight
                visible: stage.showCoverInfo
                color: stage.theme.text
                font.family: stage.theme.fontFamily
                font.weight: Font.Bold
                font.pixelSize: Math.round(stage.fontSize * 0.62)
            }
            Text {
                width: parent.width
                text: Player.artist + (Player.album ? " — " + Player.album : "")
                elide: Text.ElideRight
                visible: stage.showCoverInfo
                color: stage.theme.text
                opacity: 0.6
                font.family: stage.theme.fontFamily
                font.pixelSize: Math.round(stage.fontSize * 0.45)
            }
            Item { width: 1; height: stage.fontSize * 0.6; visible: stage.showCoverInfo }
        }

        Text { // previous
            width: parent.width
            text: (stage.lines.revision, stage.lineText(stage.centerIndex - 1))
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            color: stage.theme.text
            opacity: 0.3 + 0.25 * stage.slide
            font.family: stage.theme.fontFamily
            font.weight: stage.theme.fontWeight
            font.pixelSize: Math.round(stage.fontSize * 0.7)
        }
        Column { // current
            width: parent.width
            spacing: stage.fontSize * 0.25
            Text {
                width: parent.width
                text: (stage.lines.revision, stage.centerIndex >= 0 ? stage.lineText(stage.centerIndex) : "…")
                wrapMode: Text.Wrap
                color: stage.theme.text
                opacity: stage.cursor.activeIndex >= 0 ? 1 : 0.6
                Behavior on opacity { NumberAnimation { duration: 400 } }
                font.family: stage.theme.fontFamily
                font.weight: stage.theme.fontWeight
                font.pixelSize: Math.round(stage.fontSize * (1.0 - 0.15 * stage.slide))
            }
            Text {
                width: parent.width
                readonly property string tr: (stage.lines.revision, stage.centerIndex >= 0 ? (stage.lines.get(stage.centerIndex).translation ?? "") : "")
                visible: stage.showTranslation && tr.length > 0
                text: tr
                wrapMode: Text.Wrap
                color: stage.theme.accent
                opacity: 0.85
                font.family: stage.theme.fontFamily
                font.pixelSize: Math.round(stage.fontSize * 0.5)
            }
        }
        Text { // next
            width: parent.width
            text: (stage.lines.revision, stage.lineText(stage.centerIndex + 1))
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
            color: stage.theme.text
            opacity: 0.35
            font.family: stage.theme.fontFamily
            font.weight: stage.theme.fontWeight
            font.pixelSize: Math.round(stage.fontSize * 0.7)
        }
    }
}
