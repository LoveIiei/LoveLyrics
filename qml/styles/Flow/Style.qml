// SPDX-License-Identifier: AGPL-3.0-or-later
// Style 1 — "Flow": Apple Music-like scrolling lyrics.
//
// All lines are laid out once in a Column. One scroll target puts the focus line at
// `anchorY`; each line follows that target with its own Translate, delayed a little more
// the further down it is, so the list moves like a wave. Only scale / opacity / translate
// / blur animate, never font sizes, so text is not re-laid out per frame.
import QtQuick
import QtQuick.Effects
import LoveLyrics.Backend
import "../../components"

Item {
    id: flow

    // Style contract (see qml/styles/README.md)
    required property var lines           // LyricsModel
    required property var cursor          // LyricsCursor
    required property real positionMs     // lyrics position (offsets applied)
    required property var theme           // Theme (animated palette + font settings)
    required property url coverSource
    required property var styleSettings   // {blur, stagger, anchor}
    property bool showTranslation: true
    property bool showCoverInfo: true
    property bool animate: true

    readonly property real blurStrength: styleSettings.blur ?? 1.0
    readonly property int staggerMs: styleSettings.stagger ?? 45
    readonly property real anchorY: height * (styleSettings.anchor ?? 0.36)

    readonly property real baseFont: Math.max(24, Math.min(width * 0.042, height * 0.072)) * theme.fontScale
    readonly property int focusIndex: lines.synced ? cursor.focusIndex : -1
    readonly property int activeIndex: lines.synced ? cursor.activeIndex : -1

    // 0 = jump instantly, 1 = quick move (seek), 2 = wave
    property int moveMode: 0
    property real scrollTarget: anchorY
    property int lastFocus: -2

    function targetFor(index) {
        if (!lines.synced) {
            const dur = Math.max(1, Clock.durationMs)
            const travel = Math.max(0, column.height - flow.height * 0.5)
            return flow.height * 0.3 - travel * Math.min(1, Math.max(0, flow.positionMs / dur))
        }
        if (index < 0) {
            const first = repeater.itemAt(0)
            return flow.anchorY + flow.baseFont * 1.2 - (first ? first.y : 0)
        }
        const item = repeater.itemAt(index)
        return item ? flow.anchorY - item.y : flow.scrollTarget
    }

    function retarget(animated) {
        const jump = Math.abs(focusIndex - lastFocus) > 2 || lastFocus === -2
        lastFocus = focusIndex
        moveMode = !animated ? 0 : (jump ? 1 : 2)
        scrollTarget = targetFor(focusIndex)
    }

    onFocusIndexChanged: retarget(true)
    onWidthChanged: Qt.callLater(retarget, false)
    onHeightChanged: Qt.callLater(retarget, false)
    onShowTranslationChanged: Qt.callLater(retarget, false)
    onBaseFontChanged: Qt.callLater(retarget, false)
    onPositionMsChanged: if (!lines.synced) { moveMode = 0; scrollTarget = targetFor(-1) }

    Connections {
        target: flow.lines
        function onDocumentChanged() {
            flow.lastFocus = -2
            Qt.callLater(flow.retarget, false)
            fadeIn.restart()
        }
    }
    Component.onCompleted: Qt.callLater(retarget, false)

    NumberAnimation {
        id: fadeIn
        target: column
        property: "opacity"
        from: 0
        to: 1
        duration: 700
        easing.type: Easing.OutCubic
    }

    Column {
        id: column
        x: Math.max(flow.width * 0.09, (flow.width - width) / 2)
        width: Math.min(flow.width * 0.82, flow.baseFont * 26)
        spacing: flow.baseFont * 0.62
        // Text wrapping settles after the first layout pass; follow it without animating.
        onHeightChanged: Qt.callLater(flow.retarget, false)

        Repeater {
            id: repeater
            model: flow.lines

            delegate: Item {
                id: line
                required property int index
                required property string text
                required property string translation
                required property bool isInstrumental
                required property real startMs
                required property real endMs

                width: column.width
                height: body.height

                readonly property int distance: flow.focusIndex < 0 ? index + 1 : Math.abs(index - flow.focusIndex)
                readonly property bool isActive: index === flow.activeIndex
                readonly property bool isFocus: index === flow.focusIndex
                readonly property bool isPast: flow.focusIndex >= 0 && index < flow.focusIndex
                readonly property real blurAmount: flow.lines.synced
                    ? Math.min(1, Math.max(0, distance - (isFocus ? 1 : 0)) * 0.16) * flow.blurStrength : 0

                // Displayed scroll offset; follows flow.scrollTarget with a per-line delay.
                property real lag: flow.scrollTarget
                readonly property int delay: flow.staggerMs * Math.max(0, Math.min(8, index - flow.focusIndex + 1))

                Connections {
                    target: flow
                    function onScrollTargetChanged() {
                        if (flow.moveMode === 2 && line.delay > 0) {
                            lagTimer.interval = line.delay
                            lagTimer.restart()
                        } else {
                            lagTimer.stop()
                            line.lag = flow.scrollTarget
                        }
                    }
                }
                Timer {
                    id: lagTimer
                    onTriggered: line.lag = flow.scrollTarget
                }
                Behavior on lag {
                    enabled: flow.moveMode > 0
                    NumberAnimation {
                        duration: flow.moveMode === 1 ? 450 : 820
                        // Slight overshoot, like a soft spring.
                        easing.type: Easing.BezierSpline
                        easing.bezierCurve: flow.moveMode === 1 ? [0.2, 0.8, 0.3, 1, 1, 1] : [0.22, 1.1, 0.36, 1, 1, 1]
                    }
                }

                transform: Translate { y: line.lag }
                // Lines far off screen are not drawn. (The delegate itself stays visible:
                // Column skips invisible children when laying out.)
                readonly property bool onScreen: line.y + line.lag + line.height > -flow.height * 0.25
                                                 && line.y + line.lag < flow.height * 1.25
                // Soft fade towards the top (under the track badge) and bottom edges.
                readonly property real screenY: line.y + line.lag
                opacity: Math.min(1, Math.max(0, (screenY + line.height * 0.3 - flow.height * 0.08) / (flow.height * 0.16)))
                         * Math.min(1, Math.max(0, (flow.height - screenY) / (flow.height * 0.22)))

                Item {
                    id: body
                    visible: line.onScreen
                    width: parent.width
                    height: textColumn.height
                    transformOrigin: Item.Left
                    scale: line.isActive || (line.isInstrumental && line.isFocus) ? 1.0 : 0.94
                    opacity: !flow.lines.synced ? 0.85
                             : line.isActive ? 1.0
                             : line.isFocus ? 0.55
                             : Math.max(0.12, (line.isPast ? 0.32 : 0.42) - 0.045 * line.distance)
                    Behavior on scale { NumberAnimation { duration: 650; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 500; easing.type: Easing.OutQuad } }

                    layer.enabled: line.onScreen && line.blurAmount > 0.02
                    layer.effect: MultiEffect {
                        blurEnabled: true
                        blur: line.blurAmount
                        blurMax: 24
                        autoPaddingEnabled: true
                    }

                    Column {
                        id: textColumn
                        width: parent.width
                        spacing: flow.baseFont * 0.18

                        Text {
                            width: parent.width
                            visible: !line.isInstrumental
                            text: line.text
                            wrapMode: Text.Wrap
                            color: flow.theme.text
                            font.family: flow.theme.fontFamily
                            font.weight: flow.theme.fontWeight
                            font.pixelSize: Math.round(flow.baseFont)
                            lineHeight: 1.08
                        }
                        Text {
                            width: parent.width
                            visible: flow.showTranslation && line.translation.length > 0 && !line.isInstrumental
                            text: line.translation
                            wrapMode: Text.Wrap
                            color: flow.theme.text
                            opacity: 0.72
                            font.family: flow.theme.fontFamily
                            font.weight: Font.Medium
                            font.pixelSize: Math.round(flow.baseFont * 0.5)
                        }
                        InstrumentalDots {
                            visible: line.isInstrumental
                            dotSize: flow.baseFont * 0.42
                            color: flow.theme.text
                            active: line.isActive
                            progress: line.isActive ? flow.cursor.progress : 0
                            positionMs: line.isActive ? flow.positionMs : 0
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: flow.lines.synced && !line.isActive
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Player.setPositionMs(Math.max(0, line.startMs - Clock.totalOffsetMs + 5))
                }
            }
        }
    }

    TrackBadge {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Math.max(24, flow.height * 0.04)
        visible: flow.showCoverInfo
        theme: flow.theme
        coverSource: flow.coverSource
        size: Math.max(56, flow.height * 0.085)
    }
}
