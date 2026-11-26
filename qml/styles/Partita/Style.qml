// SPDX-License-Identifier: AGPL-3.0-or-later
// Style "Partita" (云阶, "cloud steps") — after Folia's Partita mode (AGPL-3.0,
// chthollyphile/folia-major, src/components/visualizer/partita/).
//
// The current line is split into rows of uneven length, staggered left / right like steps.
// A row appears when its first word is sung: it springs in from its side while an L-shaped
// guide line draws in at its corner. Words wait invisible, scattered, turned and blurred;
// on their timestamp they spring into place at 1.4× in the accent colour with a glow, then
// settle back and drift. On chorus lines each word throws off a ripple.
//
// Every motion is a function of the playback position (no stateful animations), so seeking
// and pausing always show the right frame.
import QtQuick
import QtQuick.Effects
import LoveLyrics.Backend
import "../../components"
import "partita.js" as P

Item {
    id: partita

    required property var lines
    required property var cursor
    required property real positionMs
    required property var theme
    required property url coverSource
    required property var styleSettings
    property bool showTranslation: true
    property bool showCoverInfo: true
    property bool animate: true

    readonly property real staggerMax: styleSettings.stagger ?? 100
    readonly property real staggerMin: Math.min(20, staggerMax)
    readonly property real motion: styleSettings.motion ?? 1.0
    readonly property bool guideLines: styleSettings.guides ?? true
    readonly property real unit: height / 1080 // design px -> screen px

    readonly property real baseFont: Math.round(Math.max(18, height * 0.042) * theme.fontScale)
    readonly property int maxRows: Math.max(1, Math.floor(height * 0.62 / (baseFont * 2.1)))

    readonly property int displayIndex: lines.synced ? (cursor.activeIndex >= 0 ? cursor.activeIndex : cursor.focusIndex) : -1
    readonly property color accent: theme.accent
    readonly property color baseColor: theme.text

    property int current: 0
    function show(index) {
        const incoming = current === 0 ? slotB : slotA
        const outgoing = current === 0 ? slotA : slotB
        if (outgoing.lineIndex === index && outgoing.lineIndex >= 0)
            return
        outgoing.leave()
        incoming.enter(index)
        current = current === 0 ? 1 : 0
    }
    onDisplayIndexChanged: show(displayIndex)
    onMaxRowsChanged: { slotA.rebuild(); slotB.rebuild() }
    onStyleSettingsChanged: { slotA.rebuild(); slotB.rebuild() }
    Connections {
        target: partita.lines
        function onDocumentChanged() {
            slotA.clear()
            slotB.clear()
            partita.show(partita.displayIndex)
        }
    }
    Component.onCompleted: show(displayIndex)

    function mix(a, b, t) {
        t = P.clamp01(t)
        return Qt.rgba(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t)
    }

    // ---- one line ----------------------------------------------------------------------
    component LineSlot: Item {
        id: slot
        anchors.fill: parent
        property int lineIndex: -1
        property var line: ({})
        property var rows: []
        readonly property bool isInstrumental: line.isInstrumental === true
        readonly property bool isChorus: line.isChorus === true

        function enter(index) {
            leaveAnim.stop()
            lineIndex = index
            line = index >= 0 ? partita.lines.get(index) : ({})
            opacity = 1
            scale = 1
            rebuild()
        }
        function rebuild() {
            if (lineIndex < 0 || isInstrumental) {
                rows = []
                return
            }
            const units = partita.lines.units(lineIndex)
            rows = P.buildRows(units, partita.maxRows, line.startMs / 1000, partita.staggerMin * partita.unit,
                               partita.staggerMax * partita.unit, partita.motion)
        }
        function leave() {
            if (lineIndex >= 0)
                leaveAnim.restart()
        }
        function clear() {
            leaveAnim.stop()
            lineIndex = -1
            rows = []
            opacity = 0
        }

        ParallelAnimation {
            id: leaveAnim
            NumberAnimation { target: slot; property: "opacity"; to: 0; duration: 380; easing.type: Easing.InQuad }
            NumberAnimation { target: slot; property: "scale"; to: 0.96; duration: 400; easing.type: Easing.OutQuad }
            onFinished: { slot.lineIndex = -1; slot.rows = [] }
        }

        Column {
            id: rowColumn
            anchors.centerIn: parent
            anchors.verticalCenterOffset: translationText.visible ? -translationText.height / 2 : 0
            spacing: partita.baseFont * 0.75

            Repeater {
                model: slot.rows
                delegate: Item {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property var units: modelData.units
                    readonly property real rowStart: units.length ? units[0].startMs - 80 : 0
                    readonly property real rowEnd: units.length ? units[units.length - 1].endMs : 0
                    readonly property real tRow: (partita.positionMs - rowStart) / 1000
                    readonly property bool shown: tRow >= 0
                    readonly property real s: P.spring(tRow)
                    readonly property bool active: partita.positionMs >= rowStart && partita.positionMs <= rowEnd
                    readonly property bool passed: partita.positionMs > rowEnd

                    width: words.width
                    height: words.height
                    x: (rowColumn.width - width) / 2
                    // The column is as wide as its widest row; rows are centred, then staggered.
                    opacity: shown ? P.clamp01(tRow / 0.1) : 0
                    visible: shown
                    scale: 0.85 + 0.15 * s
                    rotation: modelData.rotate
                    transform: Translate { x: row.modelData.stagger + (1 - row.s) * (row.modelData.left ? -40 : 40) * partita.unit }

                    // L-shaped guide line at the row's outer corner.
                    readonly property real grow: P.easeOut(tRow / 0.4)
                    readonly property color guideColor: active ? partita.accent
                                                        : passed ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(1, 1, 1, 0.14)
                    Rectangle { // vertical stroke
                        visible: partita.guideLines
                        width: Math.max(1, partita.unit * 1.2)
                        height: 32 * partita.unit
                        x: row.modelData.left ? -8 * partita.unit : row.width + 8 * partita.unit - width
                        y: row.height + 16 * partita.unit - height
                        color: row.guideColor
                        transform: Scale { origin.y: 32 * partita.unit; yScale: row.grow }
                    }
                    Rectangle { // horizontal stroke
                        visible: partita.guideLines
                        height: Math.max(1, partita.unit * 1.2)
                        width: row.width + 36 * partita.unit
                        x: row.modelData.left ? -16 * partita.unit : row.width + 16 * partita.unit - width
                        y: row.height + 8 * partita.unit
                        color: row.guideColor
                        transform: Scale { origin.x: row.modelData.left ? 0 : row.width + 36 * partita.unit; xScale: row.grow }
                    }

                    Row {
                        id: words
                        spacing: 0
                        Repeater {
                            model: row.units
                            delegate: Item {
                                id: word
                                required property var modelData
                                required property int index
                                readonly property bool cjk: P.isCjk(modelData.text)
                                readonly property var cfg: P.wordConfig(modelData.startMs, index, partita.motion)
                                readonly property real rowScale: row.modelData.scale
                                readonly property real start: modelData.startMs
                                readonly property real end: Math.max(modelData.endMs, modelData.startMs + 120)
                                readonly property real tIn: (partita.positionMs - (start - 80)) / 1000
                                readonly property real tOut: (partita.positionMs - end) / 1000
                                readonly property bool shown: tIn >= 0
                                readonly property real s: P.spring(tIn)
                                readonly property real settle: P.easeOut(tOut / 0.5)
                                // Folia uses 1.4 and has an opt-in "avoid overlap"; here room is always
                                // reserved (see width) and the active size is a little smaller.
                                readonly property real activeScale: rowScale * 1.3
                                readonly property real gap: modelData.spaceAfter
                                                            ? partita.baseFont * rowScale * 0.32
                                                            : (cjk ? partita.baseFont * rowScale * 0.06 : 0)

                                readonly property real room: body.implicitWidth * 0.18
                                width: body.implicitWidth + room + gap
                                height: body.implicitHeight
                                visible: shown

                                Item {
                                    id: glyphs
                                    x: word.room / 2
                                    width: body.implicitWidth
                                    height: body.implicitHeight
                                    opacity: word.tOut > 0 ? 1 - 0.18 * word.settle : P.clamp01(word.tIn / 0.1)
                                    scale: word.tOut > 0
                                           ? word.activeScale + (word.rowScale - word.activeScale) * word.settle
                                           : 0.5 + (word.activeScale - 0.5) * word.s
                                    rotation: word.cfg.rotate + (1 - word.s) * 20 * Math.min(1, partita.motion)
                                              + (word.tOut > 0 ? word.cfg.passedRotate * P.clamp01(word.tOut / 5) : 0)
                                    transform: Translate {
                                        x: (word.cfg.x + (1 - word.s) * word.cfg.fromX * Math.min(1.5, partita.motion)) * partita.unit
                                        y: (word.cfg.y + (1 - word.s) * word.cfg.fromY * Math.min(1.5, partita.motion)) * partita.unit
                                    }

                                    // Glow under the glyphs: lights up on the word's beat, fades after.
                                    readonly property real glow: word.tIn < 0 ? 0
                                        : word.tOut > 0 ? 1 - P.clamp01(word.tOut / 0.45)
                                        : P.clamp01(word.tIn / 0.15)
                                    Text {
                                        id: glowText
                                        anchors.fill: parent
                                        visible: glyphs.glow > 0.01
                                        text: word.modelData.text
                                        color: partita.accent
                                        opacity: glyphs.glow * 0.9
                                        font: body.font
                                        layer.enabled: visible
                                        layer.effect: MultiEffect {
                                            blurEnabled: true
                                            blur: 0.7
                                            blurMax: 24
                                            brightness: 0.3
                                            autoPaddingEnabled: true
                                        }
                                    }
                                    // Chorus ripple: a ghost that grows and fades as the word lands.
                                    Text {
                                        anchors.centerIn: parent
                                        visible: slot.isChorus && word.tIn > 0 && word.tIn < 0.7
                                        text: word.modelData.text
                                        color: partita.accent
                                        font: body.font
                                        opacity: 0.5 * (1 - word.tIn / 0.7)
                                        scale: 1 + (word.cfg.ripple - 1) * P.easeOut(word.tIn / 0.7)
                                    }
                                    Text {
                                        id: body
                                        text: word.modelData.text
                                        font.family: partita.theme.fontFamily
                                        font.weight: partita.theme.fontWeight
                                        font.pixelSize: Math.round(partita.baseFont * word.rowScale)
                                        color: word.tOut > 0
                                               ? partita.mix(partita.accent, partita.baseColor, word.tOut / 0.8)
                                               : partita.mix(partita.baseColor, partita.accent,
                                                             (partita.positionMs - word.start) / Math.max(1, word.end - word.start))
                                        // Blur-to-sharp in the first 0.2 s.
                                        readonly property real blurAmount: 1 - P.clamp01(word.tIn / 0.2)
                                        layer.enabled: blurAmount > 0.02
                                        layer.effect: MultiEffect {
                                            blurEnabled: true
                                            blur: body.blurAmount * 0.6
                                            blurMax: 20
                                            autoPaddingEnabled: true
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Text {
            id: translationText
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: rowColumn.bottom
            anchors.topMargin: partita.baseFont * 0.9
            width: Math.min(implicitWidth, partita.width * 0.8)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: partita.showTranslation && (slot.line.translation ?? "").length > 0 && slot.rows.length > 0
            text: slot.line.translation ?? ""
            color: partita.baseColor
            opacity: slot.rows.length ? 0.7 * P.clamp01((partita.positionMs - (slot.line.startMs ?? 0)) / 400) : 0
            font.family: partita.theme.fontFamily
            font.pixelSize: Math.round(partita.baseFont * 0.5)
        }

        InstrumentalDots {
            anchors.centerIn: parent
            visible: slot.isInstrumental
            dotSize: partita.baseFont * 0.45
            color: partita.baseColor
            active: slot.lineIndex === partita.cursor.activeIndex
            progress: active ? partita.cursor.progress : 0
            positionMs: partita.positionMs
        }
    }

    LineSlot { id: slotA }
    LineSlot { id: slotB }

    TrackBadge {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Math.max(24, partita.height * 0.04)
        visible: partita.showCoverInfo
        theme: partita.theme
        coverSource: partita.coverSource
        size: Math.max(48, partita.height * 0.065)
    }
}
