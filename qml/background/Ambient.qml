// SPDX-License-Identifier: AGPL-3.0-or-later
// Shared background for all styles: the cover, blurred once per track in C++ (small
// texture, scaled up here), slowly drifting and zooming (Ken Burns, ~60 s loop), tinted
// towards the palette's dark colour, with grain + vignette. Without a cover (or in
// "gradient" mode) an animated palette gradient is shown instead.
import QtQuick
import LoveLyrics.Backend

Item {
    id: root

    required property var theme
    property bool animate: true          // false when paused / hidden: no per-frame work
    property string mode: Settings.backgroundMode // "cover" | "gradient"
    property real tintStrength: 0.45

    readonly property bool useCover: mode === "cover" && Cover.hasCover

    property real phase: 0 // 0..1 over one loop
    NumberAnimation on phase {
        from: 0
        to: 1
        duration: 60000
        loops: Animation.Infinite
        running: root.animate && root.visible
    }

    Rectangle {
        anchors.fill: parent
        color: root.theme.dark
    }

    // Gradient mode / no cover.
    ShaderEffect {
        id: gradient
        anchors.fill: parent
        opacity: root.useCover ? 0 : 1
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 900 } }
        property real time: root.phase * 60.0 * 1.5
        property size resolution: Qt.size(width, height)
        property color color1: Qt.darker(root.theme.dark, 1.0)
        property color color2: Qt.darker(root.theme.vibrant, 2.2)
        property color color3: Qt.darker(root.theme.muted, 1.8)
        property color color4: Qt.darker(root.theme.dominant, 2.0)
        fragmentShader: "qrc:/shaders/gradient.frag.qsb"
    }

    // Blurred cover, Ken Burns.
    Item {
        anchors.fill: parent
        opacity: root.useCover ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 900 } }
        clip: true

        CrossfadeImage {
            id: cover
            readonly property real side: Math.max(root.width, root.height)
            width: side
            height: side
            source: Cover.blurredSource
            fillMode: Image.Stretch
            fadeMs: 1200
            // Drift on a slow Lissajous path, zooming between 1.25 and 1.4.
            readonly property real angle: root.phase * Math.PI * 2
            scale: 1.32 + 0.07 * Math.sin(angle)
            x: (root.width - width) / 2 + root.width * 0.05 * Math.sin(angle)
            y: (root.height - height) / 2 + root.height * 0.05 * Math.cos(angle * 2.0)
            rotation: 4 * Math.sin(angle)
        }
    }

    // Tint towards the palette's dark colour, so text always reads.
    Rectangle {
        anchors.fill: parent
        color: root.theme.dark
        opacity: root.useCover ? root.tintStrength : 0.15
        Behavior on opacity { NumberAnimation { duration: 900 } }
    }

    ShaderEffect {
        anchors.fill: parent
        property real time: root.animate ? root.phase * 600.0 : 0
        property real grain: 0.6
        property real vignette: 0.55
        property size resolution: Qt.size(width, height)
        fragmentShader: "qrc:/shaders/grain.frag.qsb"
    }
}
