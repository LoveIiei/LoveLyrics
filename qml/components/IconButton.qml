// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick

Item {
    id: root
    property string icon            // name in qml/icons, without .svg
    property string label           // text instead of an icon
    property real size: 44
    property bool checked: false
    property color color: "white"
    property string tip
    signal clicked()

    width: label ? Math.max(size, labelText.implicitWidth + size * 0.5) : size
    height: size

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.color
        opacity: mouse.pressed ? 0.25 : mouse.containsMouse ? 0.14 : root.checked ? 0.1 : 0
        Behavior on opacity { NumberAnimation { duration: 120 } }
    }
    Image {
        visible: root.icon.length > 0
        anchors.centerIn: parent
        width: root.size * 0.55
        height: width
        sourceSize: Qt.size(width * 2, height * 2)
        source: root.icon ? Qt.resolvedUrl("../icons/" + root.icon + ".svg") : ""
        opacity: root.enabled ? (root.checked || mouse.containsMouse ? 1 : 0.82) : 0.3
    }
    Text {
        id: labelText
        visible: root.label.length > 0
        anchors.centerIn: parent
        text: root.label
        color: root.color
        opacity: root.enabled ? 0.9 : 0.3
        font.pixelSize: Math.round(root.size * 0.34)
        font.weight: Font.DemiBold
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
