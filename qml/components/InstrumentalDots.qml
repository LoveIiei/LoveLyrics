// SPDX-License-Identifier: AGPL-3.0-or-later
// Three breathing dots for an instrumental break; they light up one by one as it passes.
import QtQuick

Row {
    id: root
    property real dotSize: 16
    property color color: "white"
    property bool active: false
    property real progress: 0   // 0..1 through the break
    property real positionMs: 0

    spacing: dotSize * 0.7
    height: dotSize * 1.6

    // Gentle breathing, ~2.4 s per breath, only while the break is active.
    readonly property real breath: active ? 0.5 + 0.5 * Math.sin(positionMs / 2400 * Math.PI * 2) : 0

    Repeater {
        model: 3
        Rectangle {
            required property int index
            anchors.verticalCenter: parent.verticalCenter
            width: root.dotSize
            height: root.dotSize
            radius: width / 2
            color: root.color
            readonly property real fill: Math.max(0, Math.min(1, root.progress * 3 - index))
            opacity: root.active ? 0.25 + 0.75 * fill : 0.35
            scale: root.active ? 0.85 + 0.2 * root.breath : 0.7
            Behavior on opacity { NumberAnimation { duration: 250 } }
        }
    }
}
