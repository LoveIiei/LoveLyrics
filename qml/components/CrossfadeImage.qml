// SPDX-License-Identifier: AGPL-3.0-or-later
// Two Image layers: a new source loads into the hidden one and fades in when ready, so a
// track change never flashes an empty frame.
import QtQuick

Item {
    id: root

    property url source
    property int fillMode: Image.PreserveAspectCrop
    property int fadeMs: 900
    property size sourceSize: Qt.size(0, 0)
    readonly property bool ready: (front === a ? a : b).status === Image.Ready && (front === a ? a : b).source != ""

    property Image front: a

    onSourceChanged: {
        const back = front === a ? b : a
        back.source = source
        if (source == "" || back.status === Image.Ready)
            swap()
    }

    function swap() {
        const back = front === a ? b : a
        front = back
    }

    Image {
        id: a
        anchors.fill: parent
        fillMode: root.fillMode
        sourceSize: root.sourceSize
        asynchronous: true
        cache: false
        smooth: true
        opacity: root.front === a && source != "" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: root.fadeMs; easing.type: Easing.InOutQuad } }
        onStatusChanged: if (status === Image.Ready && root.front !== a && source == root.source) root.swap()
    }
    Image {
        id: b
        anchors.fill: parent
        fillMode: root.fillMode
        sourceSize: root.sourceSize
        asynchronous: true
        cache: false
        smooth: true
        opacity: root.front === b && source != "" ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: root.fadeMs; easing.type: Easing.InOutQuad } }
        onStatusChanged: if (status === Image.Ready && root.front !== b && source == root.source) root.swap()
    }
}
