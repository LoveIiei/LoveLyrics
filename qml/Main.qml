// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Window

Window {
    id: root
    width: 1280
    height: 800
    visible: true
    color: "#101014"
    title: Qt.application.displayName

    Text {
        anchors.centerIn: parent
        text: "Love Lyrics"
        color: "#ffffff"
        font.pixelSize: 48
    }
}
