// SPDX-License-Identifier: AGPL-3.0-or-later
// Style "Lumiere" (绘光, "painted light") — a stage-lighting lyric PV after Folia's Lumiere
// mode (AGPL-3.0, chthollyphile/folia-major, src/components/visualizer/lumiere/).
//
// Each section of the song gets a light rig (spot, crossed lamps, searchlights, footlights,
// backlight, pinspot, strobes, a curtain slit, lasers, a projector). Beams cut through
// drifting haze, dust glitters only inside them, and the lights dim and come up again
// between sections. Words light up as they are sung — as bright as the light that falls on
// them, computed with the same formula as the shader — and loud rigs fire cross-shaped
// light bursts on sung words.
import QtQuick
import QtQuick.Effects
import LoveLyrics.Backend
import "../../components"
import "lumiere.js" as L

Item {
    id: lum

    required property var lines
    required property var cursor
    required property real positionMs
    required property var theme
    required property url coverSource
    required property var styleSettings
    property bool showTranslation: true
    property bool showCoverInfo: true
    property bool animate: true

    readonly property real lightIntensity: styleSettings.intensity ?? 1.0
    readonly property real fogDensity: styleSettings.fog ?? 1.0
    readonly property real dustAmount: styleSettings.dust ?? 1.0
    readonly property real quality: styleSettings.quality ?? 0.5
    readonly property real unlit: styleSettings.unlit ?? 0.22
    readonly property real themeMix: styleSettings.themeColor ?? 0.3
    readonly property bool showFrame: styleSettings.frame ?? true
    readonly property bool showEcho: styleSettings.echo ?? true

    readonly property real t: positionMs / 1000
    readonly property real aspect: width / Math.max(1, height)

    // ---- program: one rig per section ----------------------------------------------------
    property var sectionProfiles: []
    function buildProgram() {
        const out = []
        let prev = ""
        for (let s = 0; s < lines.sectionCount; ++s) {
            const sec = lines.section(s)
            const p = L.profileFor(lines.seed, s, sec.kind, prev)
            out.push(p)
            prev = p.kind
        }
        sectionProfiles = out
    }
    Connections {
        target: lum.lines
        function onDocumentChanged() { lum.buildProgram() }
    }
    Component.onCompleted: buildProgram()

    readonly property int focusLine: lines.synced ? Math.max(0, cursor.focusIndex) : 0
    readonly property int sectionIndex: {
        const r = lines.revision // re-evaluate on a new song
        if (lines.count === 0) return 0
        const s = lines.get(focusLine).section
        return s >= 0 ? s : 0
    }
    readonly property var profile: sectionProfiles[sectionIndex] ?? L.PROFILES[0]
    readonly property var rig: profile.rig(aspect)
    readonly property var burst: profile.burst ?? { density: 0, size: 3, tint: [1, 1, 1] }
    readonly property var section: (lines.revision, lines.section(sectionIndex))
    readonly property var nextSection: (lines.revision, lines.section(sectionIndex + 1))
    readonly property real rigStart: (section.startMs ?? 0) / 1000 - 0.8
    // Lights come up after a section starts and dim just before the next one.
    readonly property real fade: L.smoothstep(0, 0.9, t - rigStart)
                                 * (nextSection.startMs !== undefined
                                    ? 1 - L.smoothstep(nextSection.startMs / 1000 - 0.7, nextSection.startMs / 1000, t) * 0.85
                                    : 1)
    readonly property var lightRgb: L.lightColor(theme.accent, themeMix)
    readonly property color lightColor: Qt.rgba(lightRgb[0], lightRgb[1], lightRgb[2], 1)
    readonly property var beams: L.resolveBeams(rig, t, aspect,
                                                { intensity: lightIntensity * fade, color: lightRgb, local: t - rigStart })
    function lightAtPx(x, y) { return L.lightAt(beams, x / Math.max(1, height), y / Math.max(1, height)) }

    // ---- light field (rendered at reduced resolution, then scaled up) ------------------------
    Item {
        anchors.fill: parent
        layer.enabled: true
        layer.smooth: true
        layer.textureSize: Qt.size(Math.max(64, Math.round(width * lum.quality)), Math.max(64, Math.round(height * lum.quality)))

        ShaderEffect {
            anchors.fill: parent
            readonly property var p0: L.packBeam(lum.beams[0])
            readonly property var p1: L.packBeam(lum.beams[1])
            readonly property var p2: L.packBeam(lum.beams[2])
            readonly property var p3: L.packBeam(lum.beams[3])
            readonly property var p4: L.packBeam(lum.beams[4])
            readonly property var p5: L.packBeam(lum.beams[5])
            property real time: lum.t
            property real aspect: lum.aspect
            property real beamCount: lum.beams.length
            property real octaves: lum.quality >= 0.7 ? 5 : 4
            property vector4d fogA: Qt.vector4d(lum.rig.fog.density * lum.fogDensity, lum.rig.fog.tyndallBase, lum.rig.fog.scale, lum.rig.fog.warp)
            property vector4d fogB: Qt.vector4d(lum.rig.fog.driftX, lum.rig.fog.driftY, lum.rig.fog.ambient * lum.fogDensity,
                                                (lum.profile.motes ?? 1) * lum.dustAmount)
            property vector4d glare: lum.rig.glare
                ? Qt.vector4d(lum.rig.glare.x * lum.aspect, lum.rig.glare.y, lum.rig.glare.radius, lum.rig.glare.intensity * lum.fade * lum.lightIntensity)
                : Qt.vector4d(0, 0, 0, 0)
            property vector4d glareB: Qt.vector4d(lum.rig.glare ? lum.rig.glare.streak : 0, 0, 0, 0)
            property color fogColor: Qt.rgba(lum.lightRgb[0] * 0.6, lum.lightRgb[1] * 0.6, lum.lightRgb[2] * 0.6, 1)
            property color darkColor: Qt.rgba(Qt.darker(lum.theme.dark, 1.8).r, Qt.darker(lum.theme.dark, 1.8).g,
                                              Qt.darker(lum.theme.dark, 1.8).b, 0.82)
            property vector4d b0A: p0[0]; property vector4d b0B: p0[1]; property vector4d b0C: p0[2]; property vector4d b0D: p0[3]
            property vector4d b1A: p1[0]; property vector4d b1B: p1[1]; property vector4d b1C: p1[2]; property vector4d b1D: p1[3]
            property vector4d b2A: p2[0]; property vector4d b2B: p2[1]; property vector4d b2C: p2[2]; property vector4d b2D: p2[3]
            property vector4d b3A: p3[0]; property vector4d b3B: p3[1]; property vector4d b3C: p3[2]; property vector4d b3D: p3[3]
            property vector4d b4A: p4[0]; property vector4d b4B: p4[1]; property vector4d b4C: p4[2]; property vector4d b4D: p4[3]
            property vector4d b5A: p5[0]; property vector4d b5B: p5[1]; property vector4d b5C: p5[2]; property vector4d b5D: p5[3]
            fragmentShader: "qrc:/shaders/lightfield.frag.qsb"
        }
    }

    // ---- lyric region -------------------------------------------------------------------
    readonly property var region: profile.region ?? { cx: 0.5, cy: 0.54, w: 0.72, h: 0.5 }
    readonly property real regionX: width * region.cx
    readonly property real regionY: height * region.cy
    readonly property real regionW: width * region.w
    readonly property real heroPx: Math.round(Math.max(18, height * 0.06) * theme.fontScale
                                              * Math.min(1, region.w / 0.5 + 0.2))

    // Giant faint echo of the line behind everything, lit by the light at the centre.
    Text {
        visible: lum.showEcho && lum.lines.synced
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: Math.sin(lum.t * 0.05) * lum.width * 0.03
        y: lum.regionY - height / 2
        text: {
            const r = lum.lines.revision
            const l = lum.lines.get(lum.focusLine)
            return (l.text ?? "").split(/\s+/).sort((a, b) => b.length - a.length)[0] ?? ""
        }
        color: lum.lightColor
        opacity: 0.035 + 0.1 * lum.lightAtPx(lum.width / 2, lum.regionY)
        font.family: lum.theme.fontFamily
        font.weight: Font.Black
        font.pixelSize: Math.round(lum.height * 0.3)
    }

    // The window: previous / current / next line, sliding up on each line change.
    readonly property real lineStart: lines.count > 0 && lines.synced ? lines.get(focusLine).startMs / 1000 : 0
    readonly property real slide: 1 - Math.pow(1 - L.clamp01((t - lineStart) / 0.7), 3)

    Column {
        id: window
        x: lum.regionX - width / 2
        width: lum.regionW
        y: lum.regionY - (rows.itemAt(1) ? rows.itemAt(1).y + rows.itemAt(1).height / 2 : height / 2)
           + (1 - lum.slide) * lum.heroPx * 1.6
        spacing: lum.heroPx * 0.55

        Repeater {
            id: rows
            model: 3 // previous, current, next
            delegate: Item {
                id: row
                required property int index
                readonly property int lineIndex: lum.focusLine + index - 1
                readonly property bool exists: lineIndex >= 0 && lineIndex < lum.lines.count && lum.lines.synced
                readonly property var line: exists ? (lum.lines.revision, lum.lines.get(lineIndex)) : ({})
                readonly property bool isCurrent: index === 1
                readonly property real size: isCurrent ? lum.heroPx : Math.round(lum.heroPx * 0.55)
                width: window.width
                height: exists ? block.implicitHeight : lum.heroPx * 0.6
                opacity: !exists ? 0 : isCurrent ? 1 : index === 0 ? 0.55 * (1 - 0.6 * (1 - lum.slide)) : 0.5

                Column {
                    id: block
                    width: parent.width
                    spacing: row.size * 0.2
                    visible: row.exists

                    Flow {
                        id: words
                        width: parent.width
                        // Centre the wrapped rows (Flow has no centring of its own).
                        leftPadding: Math.max(0, (width - implicitW) / 2)
                        property real implicitW: 0
                        Component.onCompleted: Qt.callLater(measure)
                        onWidthChanged: Qt.callLater(measure)
                        function measure() {
                            let w = 0
                            for (let i = 0; i < unitRepeater.count; ++i)
                                if (unitRepeater.itemAt(i)) w += unitRepeater.itemAt(i).width
                            implicitW = Math.min(width, w)
                        }
                        Repeater {
                            id: unitRepeater
                            model: row.exists && !row.line.isInstrumental ? (lum.lines.revision, lum.lines.units(row.lineIndex)) : []
                            onCountChanged: Qt.callLater(words.measure)
                            delegate: Item {
                                id: unit
                                required property var modelData
                                required property int index
                                readonly property real progress: L.clamp01((lum.positionMs - modelData.startMs) / Math.max(80, modelData.endMs - modelData.startMs))
                                readonly property var centre: (lum.t, unit.mapToItem(lum, width / 2, height / 2))
                                readonly property real light: lum.lightAtPx(centre.x, centre.y)
                                readonly property real lit: progress * (0.32 + 0.68 * light)
                                readonly property real age: lum.t - modelData.startMs / 1000
                                // Flash when sung: 0.15 s rise, then decay (reveal.ts flashEnvelope).
                                readonly property real flash: age <= 0 ? 0 : age < 0.15 ? L.smoothstep(0, 0.15, age) : Math.exp(-(age - 0.15) / 0.5)
                                readonly property bool bursts: row.isCurrent && L.hash11(modelData.startMs * 0.37 + index) < lum.burst.density
                                width: glyph.implicitWidth + (modelData.spaceAfter ? row.size * 0.3 : row.size * 0.04)
                                height: glyph.implicitHeight

                                Text {
                                    id: glyph
                                    text: unit.modelData.text
                                    font.family: lum.theme.fontFamily
                                    font.weight: row.isCurrent ? Font.DemiBold : Font.Normal
                                    font.pixelSize: row.size
                                    color: Qt.tint(lum.theme.text, Qt.rgba(lum.lightRgb[0], lum.lightRgb[1], lum.lightRgb[2], 0.35 * unit.lit))
                                    opacity: lum.unlit + (1 - lum.unlit) * Math.min(1, unit.lit + 0.25 * unit.flash * unit.progress)
                                }
                                // Cross-shaped light burst (EVA-style) on chosen sung words.
                                Item {
                                    visible: unit.bursts && unit.flash > 0.02
                                    anchors.centerIn: glyph
                                    opacity: unit.flash
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: Math.max(2, row.size * 0.06)
                                        height: row.size * lum.burst.size * (0.6 + 0.4 * unit.flash)
                                        gradient: Gradient {
                                            GradientStop { position: 0; color: "transparent" }
                                            GradientStop { position: 0.5; color: Qt.rgba(lum.lightRgb[0] * lum.burst.tint[0],
                                                                                            lum.lightRgb[1] * lum.burst.tint[1],
                                                                                            lum.lightRgb[2] * lum.burst.tint[2], 1) }
                                            GradientStop { position: 1; color: "transparent" }
                                        }
                                    }
                                    Rectangle {
                                        anchors.centerIn: parent
                                        height: Math.max(2, row.size * 0.05)
                                        width: row.size * 1.4
                                        gradient: Gradient {
                                            orientation: Gradient.Horizontal
                                            GradientStop { position: 0; color: "transparent" }
                                            GradientStop { position: 0.5; color: "white" }
                                            GradientStop { position: 1; color: "transparent" }
                                        }
                                    }
                                }
                            }
                        }
                        // Lit words glow (one layer for the current line only).
                        layer.enabled: row.isCurrent
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowColor: lum.lightColor
                            shadowBlur: 0.9
                            shadowOpacity: 0.75
                            shadowHorizontalOffset: 0
                            shadowVerticalOffset: 0
                            autoPaddingEnabled: true
                        }
                    }
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        visible: lum.showTranslation && (row.line.translation ?? "").length > 0
                        text: row.line.translation ?? ""
                        color: lum.theme.text
                        opacity: 0.55
                        font.family: lum.theme.fontFamily
                        font.pixelSize: Math.round(row.size * 0.5)
                    }
                    InstrumentalDots {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: row.line.isInstrumental === true
                        dotSize: row.size * 0.4
                        color: lum.lightColor
                        active: row.isCurrent && lum.cursor.activeIndex === row.lineIndex
                        progress: active ? lum.cursor.progress : 0
                        positionMs: lum.positionMs
                    }
                }
            }
        }
    }

    // Viewfinder frame: corner marks and a centre cross (Folia's standard line art, simplified).
    Item {
        anchors.fill: parent
        visible: lum.showFrame
        opacity: 0.3 * Math.max(0.4, lum.fade)
        readonly property real l: lum.width * 0.07
        readonly property real r: lum.width * 0.93
        readonly property real tp: lum.height * 0.16
        readonly property real b: lum.height * 0.84
        readonly property real arm: lum.height * 0.035
        readonly property real th: Math.max(1, lum.height * 0.0015)
        Repeater {
            model: [[0, 0], [1, 0], [0, 1], [1, 1]]
            Item {
                required property var modelData
                readonly property real px: modelData[0] ? parent.r : parent.l
                readonly property real py: modelData[1] ? parent.b : parent.tp
                Rectangle { x: parent.px - (parent.modelData[0] ? parent.parent.arm : 0); y: parent.py; width: parent.parent.arm; height: parent.parent.th; color: lum.lightColor }
                Rectangle { x: parent.px; y: parent.py - (parent.modelData[1] ? parent.parent.arm : 0); width: parent.parent.th; height: parent.parent.arm; color: lum.lightColor }
            }
        }
        Rectangle { x: lum.width / 2 - width / 2; y: parent.tp - parent.arm * 0.6; width: parent.th; height: parent.arm * 0.6; color: lum.lightColor }
        Rectangle { x: lum.width / 2 - width / 2; y: parent.b; width: parent.th; height: parent.arm * 0.6; color: lum.lightColor }
    }

    TrackBadge {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Math.max(24, lum.height * 0.04)
        visible: lum.showCoverInfo
        theme: lum.theme
        coverSource: lum.coverSource
        size: Math.max(48, lum.height * 0.065)
    }
}
