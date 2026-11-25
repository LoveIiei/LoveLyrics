// SPDX-License-Identifier: AGPL-3.0-or-later
// Brief message ("Lyrics offset +200 ms") that fades out by itself.
import QtQuick

Rectangle {
    id: root
    property color textColor: "white"

    function show(message) {
        label.text = message
        opacity = 1
        hideTimer.restart()
    }

    width: label.implicitWidth + 40
    height: label.implicitHeight + 20
    radius: height / 2
    color: "#99000000"
    opacity: 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: 250 } }

    Text {
        id: label
        anchors.centerIn: parent
        color: root.textColor
        font.pixelSize: 17
        font.weight: Font.Medium
    }
    Timer {
        id: hideTimer
        interval: 1600
        onTriggered: root.opacity = 0
    }
}
