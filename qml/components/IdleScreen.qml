// SPDX-License-Identifier: AGPL-3.0-or-later
// Shown when the player isn't running (or nothing is playing): a quiet clock.
// Updates once a second, so the window only renders once a second.
import QtQuick
import LoveLyrics.Backend

Item {
    id: root
    required property var theme
    property string message: ""

    property date now: new Date()
    Timer {
        interval: 1000
        running: root.visible
        repeat: true
        triggeredOnStart: true
        onTriggered: root.now = new Date()
    }

    Column {
        anchors.centerIn: parent
        spacing: root.height * 0.02
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Qt.formatTime(root.now, Qt.locale().timeFormat(Locale.ShortFormat))
            color: root.theme.text
            opacity: 0.9
            font.family: root.theme.fontFamily
            font.weight: Font.Light
            font.pixelSize: Math.round(Math.max(48, root.height * 0.16))
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.message
            color: root.theme.text
            opacity: 0.5
            font.family: root.theme.fontFamily
            font.pixelSize: Math.round(Math.max(16, root.height * 0.025))
        }
    }
}
