// SPDX-License-Identifier: AGPL-3.0-or-later
// Cover art with rounded corners (mask) and an optional soft shadow; cross-fades on change.
import QtQuick
import QtQuick.Effects

Item {
    id: root
    property url source
    property real radius: 12
    property bool shadow: true
    property int fadeMs: 700

    // Analytic shadow: no extra render pass.
    RectangularShadow {
        anchors.fill: parent
        visible: root.shadow
        radius: root.radius
        blur: root.height * 0.08
        offset.y: root.height * 0.03
        spread: 0
        color: Qt.rgba(0, 0, 0, 0.5)
    }

    CrossfadeImage {
        id: image
        anchors.fill: parent
        source: root.source
        fadeMs: root.fadeMs
        sourceSize: Qt.size(Math.ceil(root.width * 1.5), Math.ceil(root.height * 1.5))
        visible: false
        layer.enabled: true
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        radius: root.radius
        visible: false
        layer.enabled: true
        layer.smooth: true
    }

    MultiEffect {
        anchors.fill: parent
        source: image
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }
}
