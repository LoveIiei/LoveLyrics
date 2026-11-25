// SPDX-License-Identifier: AGPL-3.0-or-later
// Small cover + title / artist, for a corner of the screen.
import QtQuick
import LoveLyrics.Backend

Row {
    id: root
    required property var theme
    property url coverSource
    property real size: 72

    spacing: size * 0.25
    opacity: visible ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 300 } }

    RoundedImage {
        width: root.size
        height: root.size
        radius: root.size * 0.12
        source: root.coverSource
        visible: Cover.hasCover
    }

    Column {
        anchors.verticalCenter: parent.verticalCenter
        spacing: root.size * 0.04
        width: Math.min(implicitWidth, root.size * 6)
        Text {
            width: Math.min(implicitWidth, root.size * 6)
            text: Player.title
            elide: Text.ElideRight
            color: root.theme.text
            font.family: root.theme.fontFamily
            font.weight: Font.DemiBold
            font.pixelSize: Math.round(root.size * 0.26)
        }
        Text {
            width: Math.min(implicitWidth, root.size * 6)
            text: Player.artist + (Player.album ? " — " + Player.album : "")
            elide: Text.ElideRight
            color: root.theme.text
            opacity: 0.65
            font.family: root.theme.fontFamily
            font.pixelSize: Math.round(root.size * 0.2)
        }
    }
}
