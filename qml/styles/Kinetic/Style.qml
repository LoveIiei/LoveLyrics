// SPDX-License-Identifier: AGPL-3.0-or-later
// Style 2 — "Kinetic": one line at a time, very large, in one of several layouts picked
// per line (seeded by the line index, so it's repeatable). Words arrive on their
// timestamps (estimated when the file has no word timing) with a fade, rise and
// blur-to-sharp; the previous line leaves with a quick fade/scale-out.
import QtQuick
import QtQuick.Effects
import LoveLyrics.Backend
import "../../components"
import "../../components/wordlayout.js" as WordLayout

Item {
    id: kinetic

    required property var lines
    required property var cursor
    required property real positionMs
    required property var theme
    required property url coverSource
    required property var styleSettings
    property bool showTranslation: true
    property bool showCoverInfo: true
    property bool animate: true

    readonly property real sizeFactor: styleSettings.size ?? 1.0
    readonly property real motion: styleSettings.motion ?? 1.0
    readonly property bool blurWords: styleSettings.blur ?? true

    // The line on screen: the active one, or (in a short gap) the one just sung.
    readonly property int displayIndex: cursor.activeIndex >= 0 ? cursor.activeIndex : cursor.focusIndex
    readonly property bool lineEnded: cursor.activeIndex < 0

    readonly property var presets: [
        { align: "left",   v: 0.42, size: 1.00, width: 0.78, tilt: -1.5 },
        { align: "center", v: 0.50, size: 1.08, width: 0.80, tilt: 0 },
        { align: "right",  v: 0.46, size: 0.95, width: 0.74, tilt: 1.5 },
        { align: "left",   v: 0.58, size: 0.92, width: 0.70, tilt: 0 },
        { align: "center", v: 0.40, size: 1.00, width: 0.76, tilt: -0.8 },
        { align: "right",  v: 0.56, size: 1.02, width: 0.78, tilt: 0.8 },
    ]
    function presetFor(index) {
        // Small integer hash so neighbouring lines differ but the choice is stable.
        let h = (index + 1) * 2654435761 % 4294967296
        h = (h ^ (h >>> 15)) >>> 0
        return presets[h % presets.length]
    }

    property int current: 0
    function show(index) {
        const incoming = current === 0 ? slotB : slotA
        const outgoing = current === 0 ? slotA : slotB
        outgoing.leave()
        incoming.enter(index)
        current = current === 0 ? 1 : 0
    }
    onDisplayIndexChanged: show(displayIndex)
    Connections {
        target: kinetic.lines
        function onDocumentChanged() {
            slotA.clear()
            slotB.clear()
            kinetic.show(kinetic.displayIndex)
        }
    }
    Component.onCompleted: show(displayIndex)

    component LineSlot: Item {
        id: slot
        anchors.fill: parent

        property int lineIndex: -1
        property var line: ({})
        property var preset: kinetic.presets[0]
        property var positions: ({ xs: [], ys: [], width: 0, height: 0 })
        readonly property bool isInstrumental: line.isInstrumental === true
        readonly property bool isCurrent: lineIndex === kinetic.displayIndex

        readonly property string plain: line.text ?? ""
        readonly property real effectiveLength: {
            let n = 0
            for (const ch of plain)
                n += ch.charCodeAt(0) > 0x2e80 ? 1.8 : 1
            return Math.max(1, n)
        }
        readonly property real fontSize: Math.round(kinetic.height * 0.11 * kinetic.sizeFactor * kinetic.theme.fontScale
                                                    * preset.size
                                                    * Math.max(0.42, Math.min(1.15, Math.sqrt(20 / effectiveLength))))
        readonly property real maxWidth: kinetic.width * preset.width
        readonly property real lineHeight: fontSize * 1.12

        function enter(index) {
            leaveAnim.stop()
            lineIndex = index
            line = index >= 0 ? kinetic.lines.get(index) : ({})
            preset = kinetic.presetFor(index)
            opacity = 1
            scale = 1
            Qt.callLater(relayout)
        }
        function leave() {
            if (lineIndex < 0)
                return
            leaveAnim.restart()
        }
        function clear() {
            leaveAnim.stop()
            lineIndex = -1
            line = ({})
            opacity = 0
        }
        function relayout() {
            const n = words.count
            const widths = [], gaps = []
            const space = fontSize * 0.26
            for (let i = 0; i < n; ++i) {
                const item = words.itemAt(i)
                if (!item)
                    return
                widths.push(item.implicitWidth)
                gaps.push(item.spaceAfter ? space : 0)
            }
            positions = WordLayout.layout(widths, gaps, maxWidth, lineHeight, preset.align)
        }
        onFontSizeChanged: Qt.callLater(relayout)
        onMaxWidthChanged: Qt.callLater(relayout)

        ParallelAnimation {
            id: leaveAnim
            NumberAnimation { target: slot; property: "opacity"; to: 0; duration: 260; easing.type: Easing.InQuad }
            NumberAnimation { target: slot; property: "scale"; to: 1.06; duration: 300; easing.type: Easing.OutQuad }
            onFinished: slot.lineIndex = -1
        }

        // The block of words, placed by the preset.
        Item {
            id: box
            width: slot.positions.width
            height: slot.positions.height + (translationText.visible ? translationText.height + slot.fontSize * 0.3 : 0)
            x: slot.preset.align === "left" ? kinetic.width * 0.09
               : slot.preset.align === "right" ? kinetic.width * 0.91 - width
               : (kinetic.width - width) / 2
            y: Math.max(kinetic.height * 0.12, Math.min(kinetic.height * 0.85 - height,
                                                         kinetic.height * slot.preset.v - height / 2))
            rotation: slot.preset.tilt * kinetic.motion
            opacity: slot.isCurrent && kinetic.lineEnded ? 0.45 : 1
            Behavior on opacity { NumberAnimation { duration: 400 } }
            visible: !slot.isInstrumental

            Repeater {
                id: words
                model: slot.lineIndex >= 0 && !slot.isInstrumental ? slot.line.words : []
                onCountChanged: Qt.callLater(slot.relayout)

                delegate: Text {
                    id: word
                    required property var modelData
                    required property int index
                    readonly property bool spaceAfter: /\s$/.test(modelData.text)
                    text: modelData.text.trim()
                    x: slot.positions.xs[index] ?? 0
                    y: slot.positions.ys[index] ?? 0
                    color: kinetic.theme.text
                    font.family: kinetic.theme.fontFamily
                    font.weight: kinetic.theme.fontWeight
                    font.pixelSize: slot.fontSize

                    // 0 -> 1 as the word arrives.
                    readonly property real appear: Math.max(0, Math.min(1,
                        (kinetic.positionMs - modelData.startMs + 60) / (260 + 120 * kinetic.motion)))
                    readonly property real eased: 1 - Math.pow(1 - appear, 3)
                    opacity: eased
                    transform: Translate { y: (1 - word.eased) * slot.fontSize * 0.35 * kinetic.motion }

                    layer.enabled: kinetic.blurWords && appear > 0 && appear < 1
                    layer.effect: MultiEffect {
                        blurEnabled: true
                        blur: (1 - word.eased) * 0.9
                        blurMax: 32
                        autoPaddingEnabled: true
                    }
                }
            }

            Text {
                id: translationText
                y: slot.positions.height + slot.fontSize * 0.3
                width: Math.max(slot.positions.width, Math.min(implicitWidth, slot.maxWidth))
                visible: kinetic.showTranslation && (slot.line.translation ?? "").length > 0
                text: slot.line.translation ?? ""
                wrapMode: Text.Wrap
                horizontalAlignment: slot.preset.align === "right" ? Text.AlignRight
                                     : slot.preset.align === "center" ? Text.AlignHCenter : Text.AlignLeft
                color: kinetic.theme.text
                opacity: words.count > 0 && words.itemAt(0) ? 0.72 * words.itemAt(0).eased : 0
                font.family: kinetic.theme.fontFamily
                font.weight: Font.Medium
                font.pixelSize: Math.round(Math.max(18, slot.fontSize * 0.36))
            }
        }

        InstrumentalDots {
            anchors.centerIn: parent
            visible: slot.isInstrumental
            dotSize: kinetic.height * 0.035
            color: kinetic.theme.text
            active: slot.isCurrent && !kinetic.lineEnded
            progress: active ? kinetic.cursor.progress : 0
            positionMs: kinetic.positionMs
        }
    }

    LineSlot { id: slotA }
    LineSlot { id: slotB }

    TrackBadge {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Math.max(24, kinetic.height * 0.04)
        visible: kinetic.showCoverInfo
        theme: kinetic.theme
        coverSource: kinetic.coverSource
        size: Math.max(48, kinetic.height * 0.065)
    }
}
