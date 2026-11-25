// SPDX-License-Identifier: AGPL-3.0-or-later
// A track without lyrics: large cover + title / artist. No error text.
import QtQuick
import LoveLyrics.Backend

Item {
    id: root
    required property var theme

    readonly property real coverSize: Math.min(width * 0.6, height * 0.52)

    Column {
        anchors.centerIn: parent
        spacing: root.height * 0.035

        RoundedImage {
            anchors.horizontalCenter: parent.horizontalCenter
            width: root.coverSize
            height: root.coverSize
            radius: root.coverSize * 0.04
            source: Cover.source
            visible: Cover.hasCover
        }
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: root.height * 0.008
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(implicitWidth, root.width * 0.8)
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: Player.title
                color: root.theme.text
                font.family: root.theme.fontFamily
                font.weight: Font.Bold
                font.pixelSize: Math.round(Math.max(22, root.height * 0.045))
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(implicitWidth, root.width * 0.8)
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: Player.artist
                color: root.theme.text
                opacity: 0.65
                font.family: root.theme.fontFamily
                font.pixelSize: Math.round(Math.max(16, root.height * 0.028))
            }
        }
    }
}
