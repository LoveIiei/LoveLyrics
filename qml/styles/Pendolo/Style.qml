// SPDX-License-Identifier: AGPL-3.0-or-later
// Style "Pendolo" (时计, "timepiece") — after Folia's Pendolo mode (AGPL-3.0,
// chthollyphile/folia-major, src/components/visualizer/pendolo/).
//
// A big wireframe clockwork wheel sits half off the left edge. Lyric lines are spokes on
// its right-hand arc, the current one horizontal, pointed at by the focal arrow. Every line
// change ticks the wheel one spoke on a springy escapement (overshoot, settle); the gears
// around it turn with it, the seconds gear ticks once a second and the balance wheel swings.
// The current line fills with the accent colour as it is sung.
//
// Clockwork parts are painted once (C++ ClockworkPart) and only rotated by the GPU. All
// motion is a function of the playback position.
import QtQuick
import QtQuick.Effects
import LoveLyrics.Backend
import "../../components"

Item {
    id: pendolo

    required property var lines
    required property var cursor
    required property real positionMs
    required property var theme
    required property url coverSource
    required property var styleSettings
    property bool showTranslation: true
    property bool showCoverInfo: true
    property bool animate: true

    readonly property real snap: styleSettings.snap ?? 2.0
    readonly property real activeScale: styleSettings.activeScale ?? 1.25
    readonly property real decor: styleSettings.decor ?? 0.6
    readonly property bool coverOnDial: styleSettings.coverOnDial ?? true

    readonly property real t: positionMs / 1000
    readonly property real sz: Math.min(width, height)
    readonly property real wheelR: sz * 0.42                  // main wheel base radius
    readonly property real cx: 0                        // wheel centre on the left edge
    readonly property real cy: height * 0.5
    readonly property real lyricR: wheelR + sz * 0.06
    readonly property real fontPx: Math.round(Math.max(16, sz * 0.034) * theme.fontScale)
    readonly property real textWidth: Math.max(140, Math.min(width * 0.46, width - (cx + wheelR) - 48)) / activeScale
    readonly property real stepRad: (100 * Math.PI / 180) / 8 // 100° arc, 9 lines in view

    // ---- escapement spring (stiffness 180·snap, damping 18 + 4/snap, mass 0.8) ------------
    function springStep(tau) {
        if (tau <= 0) return 0
        const k = 180 * snap, c = 18 + 4 / Math.max(0.5, snap), m = 0.8
        const w = Math.sqrt(k / m), z = c / (2 * Math.sqrt(k * m))
        if (z >= 1 || tau > 2) return tau > 2 ? 1 : 1 - Math.exp(-w * tau) * (1 + w * tau)
        const wd = w * Math.sqrt(1 - z * z)
        return 1 - Math.exp(-z * w * tau) * (Math.cos(wd * tau) + (z * w / wd) * Math.sin(wd * tau))
    }

    readonly property int target: lines.synced ? (cursor.activeIndex >= 0 ? cursor.activeIndex : cursor.focusIndex) : -1
    readonly property real targetStart: target >= 0 && target < lines.count ? lines.get(target).startMs / 1000 : 0
    readonly property real tick: springStep(t - targetStart)       // 0 -> 1 (overshoots) after a line change
    readonly property real indexValue: target < 0 ? -1 : target - 1 + tick
    readonly property real escapement: indexValue * stepRad          // gear rotation, rad
    readonly property real pulse: target >= 0 ? Math.exp(-Math.max(0, t - targetStart) / 0.4) : 0

    // Absolute angles of the lines on the arc (spaced so wrapped lines don't touch).
    property var angles: []
    function computeAngles() {
        const a = []
        let acc = 0
        for (let i = 0; i < repeater.count; ++i) {
            const item = repeater.itemAt(i)
            const h = item ? item.blockHeight : fontPx * 1.4
            if (i > 0) {
                const prev = repeater.itemAt(i - 1)
                const hp = prev ? prev.blockHeight : fontPx * 1.4
                acc += Math.max(stepRad, (hp + h + 24) / (2 * lyricR))
            }
            a.push(acc)
        }
        angles = a
    }
    readonly property real railAngle: {
        const a = angles
        if (a.length === 0) return 0
        if (target < 0) return (a[0] ?? 0) - stepRad
        const cur = a[target] ?? 0
        const prev = target > 0 ? a[target - 1] : cur - stepRad
        return prev + (cur - prev) * tick
    }
    Connections {
        target: pendolo.lines
        function onDocumentChanged() { Qt.callLater(pendolo.computeAngles) }
    }
    onFontPxChanged: Qt.callLater(computeAngles)
    onTextWidthChanged: Qt.callLater(computeAngles)
    onShowTranslationChanged: Qt.callLater(computeAngles)

    // ---- colours (Folia buildPendoloClockworkPalette) ---------------------------------------
    function alpha(c, a) { return Qt.rgba(c.r, c.g, c.b, a) }
    readonly property color primary: theme.text
    readonly property color accentCol: theme.accent
    readonly property real m: decor
    readonly property real lw: Math.max(1, sz * 0.0018)

    // ---- clockwork ---------------------------------------------------------------------
    component Part: ClockworkPart {
        property real centerX: 0
        property real centerY: 0
        property real size: 100
        x: centerX - size / 2
        y: centerY - size / 2
        width: size
        height: size
        lineWidth: pendolo.lw
    }

    ClockworkPart {
        kind: "glow"
        radius: pendolo.wheelR * 1.65
        x: pendolo.cx - radius
        y: pendolo.cy - radius
        width: radius * 2
        height: radius * 2
        fill: Qt.darker(pendolo.theme.dark, 1.2)
    }

    RoundedImage {
        visible: pendolo.coverOnDial && Cover.hasCover
        x: pendolo.cx - width / 2
        y: pendolo.cy - height / 2
        width: pendolo.wheelR * 1.76
        height: width
        radius: width / 2
        shadow: false
        source: pendolo.coverSource
        opacity: 0.42 * pendolo.m
        rotation: pendolo.escapement * 57.2958 * 0.15
    }

    Part { // guide rings + 60 ticks
        kind: "dial"
        radius: pendolo.wheelR
        centerX: pendolo.cx
        centerY: pendolo.cy
        size: pendolo.wheelR * 2 * 1.48
        subtle: pendolo.alpha(pendolo.primary, 0.15 * pendolo.m)
        accent: pendolo.alpha(pendolo.accentCol, 0.35 * pendolo.m)
        rotation: pendolo.escapement * 57.2958 * 0.2
    }
    Part { // the same dial lit in the accent colour on every line change
        kind: "dial"
        radius: pendolo.wheelR
        centerX: pendolo.cx
        centerY: pendolo.cy
        size: pendolo.wheelR * 2 * 1.48
        subtle: pendolo.alpha(pendolo.accentCol, 0.55)
        accent: pendolo.accentCol
        rotation: pendolo.escapement * 57.2958 * 0.2
        opacity: pendolo.pulse * 0.9
        visible: opacity > 0.01
    }
    Part { // escapement wheel
        kind: "main"
        radius: pendolo.wheelR
        toothDepth: pendolo.sz * 0.011
        centerX: pendolo.cx
        centerY: pendolo.cy
        size: (pendolo.wheelR + pendolo.sz * 0.02) * 2
        stroke: pendolo.alpha(pendolo.primary, 0.42 * pendolo.m)
        accent: pendolo.alpha(pendolo.accentCol, 0.58 * pendolo.m)
        subtle: pendolo.alpha(pendolo.primary, 0.18 * pendolo.m)
        fill: pendolo.alpha(pendolo.accentCol, 0.08 * pendolo.m)
        rotation: pendolo.escapement * 57.2958
    }
    Part { // sun pinion
        radius: pendolo.wheelR * 0.22
        teeth: 12
        toothDepth: pendolo.wheelR * 0.016
        centerX: pendolo.cx
        centerY: pendolo.cy
        size: radius * 2.2
        stroke: pendolo.alpha(pendolo.accentCol, 0.72 * pendolo.m)
        accent: pendolo.alpha(pendolo.accentCol, 0.65 * pendolo.m)
        rotation: -pendolo.escapement * 2.5 * 57.2958
    }
    Repeater { // planetary gears
        model: 3
        Part {
            required property int index
            readonly property real orbit: pendolo.escapement * 0.4 + index * Math.PI * 2 / 3
            radius: pendolo.wheelR * 0.16
            teeth: 14
            toothDepth: pendolo.wheelR * 0.012
            centerX: pendolo.cx + pendolo.wheelR * 0.52 * Math.cos(orbit)
            centerY: pendolo.cy + pendolo.wheelR * 0.52 * Math.sin(orbit)
            size: radius * 2.2
            stroke: pendolo.alpha(pendolo.primary, 0.24 * pendolo.m)
            accent: pendolo.alpha(pendolo.accentCol, 0.65 * pendolo.m)
            rotation: (-pendolo.escapement * 3 + index * 0.5) * 57.2958
        }
    }
    Part { // balance wheel (upper)
        radius: pendolo.wheelR * 0.28
        teeth: 20
        toothDepth: pendolo.wheelR * 0.017
        centerX: pendolo.cx + pendolo.wheelR * 0.2
        centerY: pendolo.cy - pendolo.wheelR * 0.75
        size: radius * 2.2
        stroke: pendolo.alpha(pendolo.accentCol, 0.72 * pendolo.m)
        accent: pendolo.alpha(pendolo.accentCol, 0.65 * pendolo.m)
        fill: pendolo.alpha(pendolo.accentCol, 0.1 * pendolo.m)
        rotation: pendolo.t * 2 * 0.1 * 57.2958
    }
    Part { // hairspring: swings like a balance spring
        kind: "hairspring"
        radius: pendolo.wheelR * 0.28
        centerX: pendolo.cx + pendolo.wheelR * 0.2
        centerY: pendolo.cy - pendolo.wheelR * 0.75
        size: radius * 2.2
        stroke: pendolo.alpha(pendolo.accentCol, 0.3 * pendolo.m)
        rotation: (Math.sin(pendolo.t * Math.PI * 2 * 1.25) * 0.6 + pendolo.t * 0.06) * 57.2958
    }
    Part { // transmission gear (lower)
        radius: pendolo.wheelR * 0.34
        teeth: 24
        toothDepth: pendolo.wheelR * 0.017
        spokes: 5
        geneva: true
        centerX: pendolo.cx + pendolo.wheelR * 0.32
        centerY: pendolo.cy + pendolo.wheelR * 0.78
        size: radius * 2.2
        stroke: pendolo.alpha(pendolo.primary, 0.42 * pendolo.m)
        subtle: pendolo.alpha(pendolo.primary, 0.32 * pendolo.m)
        accent: pendolo.alpha(pendolo.accentCol, 0.65 * pendolo.m)
        rotation: -pendolo.escapement * 1.4 * 57.2958
    }
    Part { // seconds gear: one tooth per second, with a little snap
        readonly property real secs: Math.floor(pendolo.t) + Math.min(1, pendolo.springStep((pendolo.t % 1) * 1.6))
        radius: pendolo.wheelR * 0.16
        teeth: 15
        toothDepth: pendolo.wheelR * 0.012
        spokes: 4
        centerX: pendolo.cx + pendolo.wheelR * 0.68
        centerY: pendolo.cy + pendolo.wheelR * 0.76
        size: radius * 2.2
        stroke: pendolo.alpha(pendolo.accentCol, 0.58 * pendolo.m)
        subtle: pendolo.alpha(pendolo.primary, 0.32 * pendolo.m)
        accent: pendolo.alpha(pendolo.accentCol, 0.65 * pendolo.m)
        fill: pendolo.alpha(pendolo.accentCol, 0.06 * pendolo.m)
        rotation: secs * 360 / 15
    }
    Part { // idler pinion between them
        radius: pendolo.wheelR * 0.08
        teeth: 10
        toothDepth: pendolo.wheelR * 0.009
        centerX: pendolo.cx + pendolo.wheelR * 0.54
        centerY: pendolo.cy + pendolo.wheelR * 0.75
        size: radius * 2.3
        stroke: pendolo.alpha(pendolo.primary, 0.28 * pendolo.m)
        accent: pendolo.alpha(pendolo.accentCol, 0.65 * pendolo.m)
        rotation: pendolo.escapement * 2.2 * 57.2958
    }

    // Focal axis and arrow pointing at the current line.
    Rectangle {
        x: pendolo.cx + pendolo.wheelR * 0.8
        y: pendolo.cy - height / 2
        width: pendolo.lyricR - Math.max(14, pendolo.wheelR * 0.025) - pendolo.wheelR * 0.8
        height: Math.max(2, pendolo.lw * 1.2)
        color: pendolo.alpha(pendolo.accentCol, 0.72 * Math.max(0.5, pendolo.m))
    }
    Rectangle {
        readonly property real s: Math.max(6, pendolo.sz * 0.008)
        x: pendolo.lyricR - Math.max(14, pendolo.wheelR * 0.025) - s * 0.3
        y: pendolo.cy - s / 2
        width: s
        height: s
        rotation: 45
        color: pendolo.alpha(pendolo.accentCol, 0.85)
    }

    // ---- lyric spokes ------------------------------------------------------------------
    Repeater {
        id: repeater
        model: pendolo.lines
        onCountChanged: Qt.callLater(pendolo.computeAngles)

        delegate: Item {
            id: line
            required property int index
            required property string text
            required property string translation
            required property bool isInstrumental
            required property bool isChorus

            readonly property real blockHeight: block.implicitHeight
            readonly property real theta: (pendolo.angles[index] ?? 0) - pendolo.railAngle
            readonly property real dist: Math.abs(index - Math.max(-1, pendolo.target))
            readonly property bool onArc: Math.abs(theta) < Math.PI / 2 && dist <= 8
            readonly property bool isTarget: index === pendolo.target
            readonly property bool isActive: isTarget && pendolo.cursor.activeIndex === index

            visible: onArc
            x: pendolo.cx + pendolo.lyricR * Math.cos(theta)
            y: pendolo.cy + pendolo.lyricR * Math.sin(theta) - height / 2
            width: pendolo.textWidth
            height: block.implicitHeight
            transformOrigin: Item.Left
            rotation: theta * 0.35 * 57.2958
            // The focal line grows to activeScale as it reaches the axis.
            readonly property real near: Math.max(0, 1 - Math.abs(theta) / pendolo.stepRad)
            scale: Math.max(0.7, 1 - dist * 0.08) + (pendolo.activeScale - 1) * near
            opacity: Math.max(0.12, Math.pow(Math.cos(theta * 0.75), 2.5) * (1 - Math.min(dist, 4) * 0.18))

            Column {
                id: block
                width: parent.width
                spacing: pendolo.fontPx * 0.15

                // One wrapped text for most lines; the current line is built from words so it
                // can fill with the accent colour as it is sung.
                Text {
                    width: parent.width
                    visible: !line.isInstrumental && !line.isTarget
                    text: line.text
                    wrapMode: Text.Wrap
                    color: pendolo.primary
                    font.family: pendolo.theme.fontFamily
                    font.pixelSize: pendolo.fontPx
                }
                Loader {
                    width: parent.width
                    active: line.isTarget && !line.isInstrumental
                    visible: active
                    sourceComponent: Flow {
                        width: parent ? parent.width : 0
                        Repeater {
                            model: pendolo.lines.units(line.index)
                            delegate: Item {
                                id: unit
                                required property var modelData
                                readonly property real fill: line.isActive
                                    ? Math.max(0, Math.min(1, (pendolo.positionMs - modelData.startMs) / Math.max(1, modelData.endMs - modelData.startMs)))
                                    : 0
                                width: base.implicitWidth + (modelData.spaceAfter ? pendolo.fontPx * 0.28 : 0)
                                height: base.implicitHeight
                                Text {
                                    id: base
                                    text: unit.modelData.text
                                    color: pendolo.primary
                                    font.family: pendolo.theme.fontFamily
                                    font.weight: Font.DemiBold
                                    font.pixelSize: pendolo.fontPx
                                }
                                Item {
                                    width: base.implicitWidth * unit.fill
                                    height: base.implicitHeight
                                    clip: true
                                    visible: unit.fill > 0
                                    Text { text: base.text; font: base.font; color: pendolo.accentCol }
                                }
                            }
                        }
                        layer.enabled: line.isActive && line.isChorus
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowColor: pendolo.accentCol
                            shadowBlur: 0.8
                            shadowOpacity: 0.8
                            shadowHorizontalOffset: 0
                            shadowVerticalOffset: 0
                            autoPaddingEnabled: true
                        }
                    }
                }
                Text {
                    visible: line.isInstrumental
                    text: "♪ ♪ ♪"
                    color: pendolo.primary
                    font.pixelSize: pendolo.fontPx
                }
                Text {
                    width: parent.width
                    visible: pendolo.showTranslation && line.translation.length > 0 && !line.isInstrumental
                    text: line.translation
                    wrapMode: Text.Wrap
                    color: pendolo.primary
                    opacity: 0.6
                    font.family: pendolo.theme.fontFamily
                    font.pixelSize: Math.round(pendolo.fontPx * 0.6)
                }
            }
        }
    }

    TrackBadge {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Math.max(24, pendolo.height * 0.04)
        visible: pendolo.showCoverInfo
        theme: pendolo.theme
        coverSource: pendolo.coverSource
        size: Math.max(48, pendolo.height * 0.065)
        layoutDirection: Qt.RightToLeft
    }
}
