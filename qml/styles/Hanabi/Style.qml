// SPDX-License-Identifier: AGPL-3.0-or-later
// Style "Hanabi" (花火) — a Japanese lyric-PV look after Folia's Hanabi mode (AGPL-3.0,
// chthollyphile/folia-major, src/components/visualizer/hanabi/).
//
// Every line is a "shot": its glyphs fly in one after another (13 entrance types) so the
// last one lands as the first is sung, sit on colour blocks (bands, tiles, pills, slants,
// markers...), react when sung (bounce, jump, glow, flash...), and leave (ascend, scatter,
// fall, dissolve...). Fireworks burst as lines start — launched from below, bigger on the
// chorus — and between shots the whole scene rises and swings, under a drifting night sky.
// Each section gets a shot family chosen from the song's seed; the picture is a function
// of the playback position, so seeking shows the right frame (fireworks included).
import QtQuick
import QtQuick.Effects
import LoveLyrics.Backend
import "../../components"
import "../../components/wordlayout.js" as WordLayout
import "hanabi.js" as H

Item {
    id: hanabi

    required property var lines
    required property var cursor
    required property real positionMs
    required property var theme
    required property url coverSource
    required property var styleSettings
    property bool showTranslation: true
    property bool showCoverInfo: true
    property bool animate: true

    readonly property real spin: styleSettings.spin ?? 1.0
    readonly property real sparkAmount: styleSettings.sparks ?? 1.0
    readonly property real coverColors: styleSettings.coverColors ?? 0.35
    readonly property real skyDarkness: styleSettings.sky ?? 0.8
    readonly property real motion: styleSettings.motion ?? 1.0

    readonly property real t: positionMs / 1000
    readonly property real shortSide: Math.min(width, height)

    // ---- per-song program --------------------------------------------------------------
    property var shots: []
    property var enterStarts: []
    property var lineData: []
    function buildProgram() {
        const n = lines.count
        const seed = lines.seed
        const s = [], e = [], d = []
        for (let i = 0; i < n; ++i) {
            const line = lines.get(i)
            d.push(line)
            if (line.isInstrumental) {
                s.push(null)
                e.push(line.startMs / 1000)
                continue
            }
            const shot = H.shotFor(seed, line, i, coverColors, theme)
            let glyphs = 0
            for (const u of lines.units(i))
                glyphs += H.graphemes(u.text).length
            const sched = H.enterSchedule(shot, glyphs, line.startMs / 1000)
            shot.schedule = sched
            s.push(shot)
            e.push(sched.start)
        }
        // Never start entering before the previous line has had its moment.
        for (let i = 1; i < n; ++i)
            e[i] = Math.max(e[i], e[i - 1] + 0.25)
        shots = s
        enterStarts = e
        lineData = d
    }
    function nextEnter(i) {
        if (i + 1 < enterStarts.length)
            return enterStarts[i + 1]
        return (lineData[i] ? lineData[i].endMs / 1000 : 0) + 2.5
    }
    // The line on screen: the last one whose entrance has begun.
    readonly property int target: {
        const e = enterStarts
        let lo = 0, hi = e.length - 1, ans = -1
        while (lo <= hi) {
            const mid = (lo + hi) >> 1
            if (e[mid] <= t) { ans = mid; lo = mid + 1 } else hi = mid - 1
        }
        return lines.synced ? ans : -1
    }

    property int current: 0
    function show(index) {
        if (slotA.lineIndex === index || slotB.lineIndex === index)
            return
        const incoming = current === 0 ? slotB : slotA
        incoming.enter(index)
        current = current === 0 ? 1 : 0
    }
    onTargetChanged: show(target)
    Connections {
        target: hanabi.lines
        function onDocumentChanged() {
            sparks.clear()
            slotA.clear()
            slotB.clear()
            hanabi.buildProgram()
            hanabi.show(hanabi.target)
        }
    }
    Component.onCompleted: { buildProgram(); show(target) }
    onWidthChanged: Qt.callLater(() => { slotA.relayout(); slotB.relayout() })
    onHeightChanged: Qt.callLater(() => { slotA.relayout(); slotB.relayout() })

    // ---- night sky ---------------------------------------------------------------------
    Rectangle {
        anchors.fill: parent
        opacity: hanabi.skyDarkness
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#03040c" }
            GradientStop { position: 0.65; color: Qt.darker(hanabi.theme.dark, 1.6) }
            GradientStop { position: 1.0; color: Qt.darker(hanabi.theme.dark, 1.1) }
        }
    }
    // Stars drifting down as the camera rises (one painted tile, shown twice).
    readonly property real riseProgress: {
        const i = Math.max(0, target)
        const q = H.clamp01((t - (enterStarts[i] ?? 0)) / 0.85)
        return i + H.easeOutCubic(q)
    }
    readonly property real starOffset: (t * 6 + riseProgress * height * 0.07) % Math.max(1, height)
    Canvas {
        id: starTile
        width: hanabi.width
        height: hanabi.height
        visible: false
        renderTarget: Canvas.Image
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            let s = 1234567
            const rnd = () => { s = (s * 16807) % 2147483647; return s / 2147483647 }
            const count = Math.round(width * height / 9000)
            for (let i = 0; i < count; ++i) {
                const r = rnd() < 0.08 ? 1.6 : 0.8
                ctx.fillStyle = Qt.rgba(1, 1, 1, 0.25 + rnd() * 0.6)
                ctx.beginPath()
                ctx.arc(rnd() * width, rnd() * height, r, 0, Math.PI * 2)
                ctx.fill()
            }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }
    Item {
        anchors.fill: parent
        clip: true
        opacity: hanabi.skyDarkness * 0.9
        ShaderEffectSource { sourceItem: starTile; width: hanabi.width; height: hanabi.height; y: hanabi.starOffset - hanabi.height }
        ShaderEffectSource { sourceItem: starTile; width: hanabi.width; height: hanabi.height; y: hanabi.starOffset }
    }

    SparkField {
        id: sparks
        anchors.fill: parent
        time: hanabi.t
        amount: hanabi.sparkAmount
        embers: Math.round(30 * hanabi.sparkAmount)
        emberColor: "#ffc98a"
    }

    // ---- effects scheduling --------------------------------------------------------------
    function sparkColors(shot) { return shot.palette.sparks }

    function burstAt(shot, index, cx, cy, kindOverride, scale, at) {
        const b = kindOverride || shot.burst
        if (!b)
            return
        const R = b.radius * shortSide * (scale || 1)
        const seed = (lines.seed ^ (index * 7919)) >>> 0
        const colors = sparkColors(shot)
        const fire = (x, y, when, s) => {
            if (shot.launch)
                sparks.launch(x + (s % 2 ? 30 : -30), height + 20, x, y, when - 0.8, 0.8, colors[0], s, index)
            sparks.burst(b.kind, x, y, when, colors, R, b.duration, b.count, 0.5, 0.35, 1, 0, s, index)
        }
        switch (shot.at) {
        case "sides":
            fire(width * 0.2, height * 0.3, at, seed)
            fire(width * 0.8, height * 0.28, at + 0.25, seed + 1)
            break
        case "above":
            fire(cx + (shot.rand - 0.5) * width * 0.25, Math.max(R * 0.6, cy - height * 0.27), at, seed)
            break
        case "sky":
            fire(width * (0.15 + shot.rand * 0.7), height * (0.18 + shot.rand * 0.15), at, seed)
            break
        case "behind":
        default:
            fire(cx, cy - height * 0.04, at, seed)
            break
        }
    }

    function scheduleInstrumental(index, line) {
        sparks.removeGroup(index)
        const kinds = ["kiku", "botan", "ring", "yanagi", "senrin", "kamuro"]
        const pal = H.PALETTES[["festival", "gold", "kawaii", "ocean", "night", "neon"][(lines.seed + index) % 6]]
        const start = line.startMs / 1000 + 0.4, end = line.endMs / 1000 - 0.8
        let k = 0
        for (let at = start; at < end && k < 14; at += 2.4, ++k) {
            const r = H.rng((lines.seed + index * 31 + k * 977) >>> 0)
            const x = width * (0.15 + r() * 0.7), y = height * (0.15 + r() * 0.3)
            sparks.launch(x + (r() - 0.5) * 60, height + 20, x, y, at - 0.9, 0.9, pal.sparks[0], k, index)
            sparks.burst(kinds[(k + index) % kinds.length], x, y, at, pal.sparks, shortSide * (0.22 + r() * 0.12),
                         kinds[(k + index) % kinds.length] === "kamuro" ? 3.4 : 2.0, 90, 0.5, 0.4, 0.6 + r() * 0.4,
                         r() * 3.14, k * 13 + index, index)
        }
    }

    // ---- one shot ------------------------------------------------------------------------
    component LineShot: Item {
        id: slot
        width: hanabi.width
        height: hanabi.height

        property int lineIndex: -1
        property var line: ({})
        property var shot: null
        property var glyphs: []       // [{ch, unit, sungStart, sungEnd, spaceAfter}]
        property var placed: []       // [{x, y, rot}] relative to the region centre
        property var blocks: []
        property real fontSize: 40
        property real cx: width / 2
        property real cy: height / 2
        property bool laidOut: false
        readonly property bool isInstrumental: line.isInstrumental === true

        readonly property real enterStart: lineIndex >= 0 ? (hanabi.enterStarts[lineIndex] ?? 0) : 0
        readonly property real exitStart: lineIndex >= 0 ? hanabi.nextEnter(lineIndex) - 0.05 : 0
        readonly property real qIn: H.clamp01((hanabi.t - enterStart) / 0.85)
        // Rises out exactly while the next shot rises in.
        readonly property real qOut: H.clamp01((hanabi.t - exitStart - 0.05) / 0.85)
        readonly property bool finished: lineIndex >= 0 && hanabi.t > exitStart + 1.4

        visible: lineIndex >= 0 && !finished
        onFinishedChanged: if (finished) Qt.callLater(clear)

        // The shot rises in with a swing, drifts, and rises out.
        readonly property real swing: shot ? shot.swing * hanabi.spin : 0
        transform: [
            Rotation {
                origin.x: slot.cx
                origin.y: slot.cy
                angle: slot.isInstrumental ? 0
                       : (1 - H.easeOutCubic(slot.qIn)) * slot.swing - H.easeInCubic(slot.qOut) * slot.swing * 0.6
                         + hanabi.spin * 2.0 * Math.max(-1, Math.min(1, (hanabi.t - (slot.enterStart + slot.exitStart) / 2)
                                                                      / Math.max(1.5, (slot.exitStart - slot.enterStart) / 2)))
            },
            Translate {
                y: slot.isInstrumental ? 0
                   : (1 - H.easeOutCubic(slot.qIn)) * hanabi.height * 0.5 - H.easeInOutCubic(slot.qOut) * hanabi.height * 0.6
            }
        ]
        opacity: 1 - slot.qOut * slot.qOut

        function clear() {
            lineIndex = -1
            line = ({})
            shot = null
            glyphs = []
            placed = []
            blocks = []
            laidOut = false
        }

        function enter(index) {
            clear()
            if (index < 0)
                return
            lineIndex = index
            line = hanabi.lineData[index] ?? hanabi.lines.get(index)
            if (line.isInstrumental) {
                hanabi.scheduleInstrumental(index, line)
                return
            }
            shot = hanabi.shots[index]
            const g = []
            for (const u of hanabi.lines.units(index)) {
                const chars = H.graphemes(u.text)
                const step = (u.endMs - u.startMs) / Math.max(1, chars.length)
                chars.forEach((ch, k) => g.push({
                    ch: ch, unit: g.length ? (k === 0 ? g[g.length - 1].unit + 1 : g[g.length - 1].unit) : 0,
                    sungStart: (u.startMs + k * step) / 1000, sungEnd: (u.startMs + (k + 1) * step) / 1000,
                    spaceAfter: k === chars.length - 1 && u.spaceAfter,
                }))
            }
            glyphs = g
            Qt.callLater(relayout)
        }

        function relayout() {
            if (!shot || glyphs.length === 0 || glyphRepeater.count !== glyphs.length)
                return
            const base = Math.round(shot.size * hanabi.shortSide * hanabi.theme.fontScale)
            fontSize = base
            const widths = []
            for (let i = 0; i < glyphs.length; ++i) {
                const item = glyphRepeater.itemAt(i)
                if (!item)
                    return
                widths.push(item.glyphWidth)
            }
            const layout = shot.layout
            const lineH = base * 1.35
            const regionW = hanabi.width * (layout === "left" || layout === "right" ? 0.62 : 0.76)
            const regionH = hanabi.height * 0.5
            cx = hanabi.width * (layout === "left" ? 0.44 : layout === "right" ? 0.56 : 0.5)
            cy = hanabi.height * 0.5

            // Units -> widths.
            const unitW = [], unitGap = []
            const unitOf = []
            glyphs.forEach((g, i) => {
                if (unitW.length <= g.unit) { unitW.push(0); unitGap.push(0) }
                unitW[g.unit] += widths[i]
                if (g.spaceAfter) unitGap[g.unit] = base * 0.3
                unitOf.push(g.unit)
            })
            const pos = new Array(glyphs.length)
            let rowOf = new Array(glyphs.length).fill(0)

            if (layout === "vertical") {
                const perCol = Math.max(1, Math.floor(regionH * 1.3 / (base * 1.08)))
                const cols = Math.ceil(glyphs.length / perCol)
                for (let i = 0; i < glyphs.length; ++i) {
                    const c = Math.floor(i / perCol), r = i % perCol
                    const inCol = Math.min(perCol, glyphs.length - c * perCol)
                    pos[i] = { x: ((cols - 1) / 2 - c) * base * 1.35, y: (r - (inCol - 1) / 2) * base * 1.08, rot: 0 }
                    rowOf[i] = c
                }
            } else {
                let unitX = [], unitY = [], unitRow = []
                if (layout === "stagger" && unitW.length >= 2) {
                    const rows = Math.min(3, Math.max(2, Math.ceil(unitW.length / 2)))
                    const per = Math.ceil(unitW.length / rows)
                    for (let r = 0; r < rows; ++r) {
                        const from = r * per, to = Math.min(unitW.length, from + per)
                        let w = 0
                        for (let k = from; k < to; ++k) w += unitW[k] + (k + 1 < to ? unitGap[k] : 0)
                        let x = -w / 2 + (r % 2 === 0 ? -1 : 1) * base * 0.9
                        for (let k = from; k < to; ++k) {
                            unitX[k] = x; unitY[k] = (r - (rows - 1) / 2) * lineH; unitRow[k] = r
                            x += unitW[k] + unitGap[k]
                        }
                    }
                } else {
                    const wide = layout === "arc" || layout === "diagonal"
                    const align = layout === "left" ? "left" : layout === "right" ? "right" : "center"
                    const L = WordLayout.layout(unitW, unitGap, wide ? hanabi.width * 0.86 : regionW, lineH, align)
                    for (let k = 0; k < unitW.length; ++k) {
                        unitX[k] = L.xs[k] - L.width / 2
                        unitY[k] = L.ys[k] - (L.height - lineH) / 2
                        unitRow[k] = Math.round(L.ys[k] / lineH)
                    }
                }
                let x = 0, lastUnit = -1
                for (let i = 0; i < glyphs.length; ++i) {
                    const u = unitOf[i]
                    if (u !== lastUnit) { x = unitX[u]; lastUnit = u }
                    pos[i] = { x: x + widths[i] / 2, y: unitY[u], rot: 0 }
                    rowOf[i] = unitRow[u]
                    x += widths[i]
                }
                // Shape the line.
                let minX = Infinity, maxX = -Infinity
                for (const p of pos) { minX = Math.min(minX, p.x); maxX = Math.max(maxX, p.x) }
                const span = Math.max(1, maxX - minX)
                if (layout === "arc") {
                    for (const p of pos) {
                        const u = (p.x - minX) / span
                        p.y -= Math.sin(Math.PI * u) * base * 0.9 - base * 0.45
                        p.rot = -Math.cos(Math.PI * u) * 0.32
                    }
                } else if (layout === "diagonal") {
                    for (const p of pos) {
                        p.y += ((p.x - minX) / span - 0.5) * base * 1.3
                        p.rot = 0.12
                    }
                }
                if (shot.block === "stair")
                    pos.forEach((p, i) => p.y += (i % 2 ? -1 : 1) * base * 0.16)
            }

            // Fit into the region.
            let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity
            pos.forEach((p, i) => {
                minX = Math.min(minX, p.x - widths[i] / 2); maxX = Math.max(maxX, p.x + widths[i] / 2)
                minY = Math.min(minY, p.y - base / 2); maxY = Math.max(maxY, p.y + base / 2)
            })
            const fit = Math.min(1, (hanabi.width * 0.86) / Math.max(1, maxX - minX), (hanabi.height * 0.62) / Math.max(1, maxY - minY))
            if (fit < 1) {
                pos.forEach(p => { p.x *= fit; p.y *= fit })
                fontSize = Math.max(12, Math.round(base * fit))
                for (let i = 0; i < widths.length; ++i) widths[i] *= fit
            }

            // Colour blocks.
            const fs = fontSize, pal = shot.palette
            const b = []
            const blockColor = k => pal.blocks[k % pal.blocks.length]
            const groupBy = keyOf => {
                const groups = {}
                pos.forEach((p, i) => {
                    const k = keyOf(i)
                    const gr = groups[k] || (groups[k] = { minX: Infinity, maxX: -Infinity, y: 0, n: 0, first: i, last: i, k: k })
                    gr.minX = Math.min(gr.minX, p.x - widths[i] / 2); gr.maxX = Math.max(gr.maxX, p.x + widths[i] / 2)
                    gr.y += p.y; gr.n += 1; gr.last = i
                })
                return Object.values(groups).map(gr => Object.assign(gr, { y: gr.y / gr.n }))
            }
            const pad = fs * 0.22
            switch (shot.block) {
            case "band":
                groupBy(i => rowOf[i]).forEach((gr, r) => b.push({ kind: "band", x: gr.minX - pad * 1.6, y: gr.y - fs * 0.62,
                    w: gr.maxX - gr.minX + pad * 3.2, h: fs * 1.24, color: blockColor(r), first: gr.first, last: gr.last, radius: 0 }))
                break
            case "outline":
                groupBy(i => rowOf[i]).forEach(gr => b.push({ kind: "outline", x: gr.minX - pad * 1.8, y: gr.y - fs * 0.72,
                    w: gr.maxX - gr.minX + pad * 3.6, h: fs * 1.44, color: pal.paper, first: gr.first, last: gr.last, radius: fs * 0.12 }))
                break
            case "pill": case "slant": case "double":
                groupBy(i => unitOf[i]).forEach((gr, k) => {
                    const rect = { x: gr.minX - pad, y: gr.y - fs * 0.6, w: gr.maxX - gr.minX + pad * 2, h: fs * 1.2,
                                   first: gr.first, last: gr.last }
                    if (shot.block === "double")
                        b.push(Object.assign({ kind: "rect", color: blockColor(k + 1), radius: 0, dx: fs * 0.12, dy: fs * 0.12 }, rect))
                    b.push(Object.assign({ kind: shot.block === "slant" ? "slant" : "rect", color: blockColor(k),
                                           radius: shot.block === "pill" ? fs * 0.6 : 0, dx: 0, dy: 0 }, rect))
                })
                break
            case "tile": case "circle": case "stair":
                pos.forEach((p, i) => {
                    if (glyphs[i].ch.trim() === "") return
                    const s = fs * (shot.block === "circle" ? 1.3 : 1.14)
                    b.push({ kind: "rect", x: p.x - Math.max(s, widths[i] + pad) / 2, y: p.y - s / 2,
                             w: Math.max(s, widths[i] + pad), h: s, color: blockColor(i), first: i, last: i,
                             radius: shot.block === "circle" ? s / 2 : fs * 0.08 })
                })
                break
            case "marker":
                pos.forEach((p, i) => b.push({ kind: "marker", x: p.x - widths[i] / 2 - 1, y: p.y + fs * 0.02,
                                                w: widths[i] + 2, h: fs * 0.4, color: blockColor(0), first: i, last: i, radius: 0 }))
                break
            }
            blocks = b
            placed = pos
            laidOut = true
            hanabi.scheduleShot(slot)
        }

        // Blocks behind the glyphs.
        Repeater {
            model: slot.blocks
            delegate: Item {
                id: blk
                required property var modelData
                readonly property var firstGlyph: glyphRepeater.itemAt(modelData.first)
                readonly property real arrive: firstGlyph ? firstGlyph.arrival : 0
                readonly property real grow: H.easeOutBack(H.clamp01((hanabi.t - arrive) / 0.3), 1.4)
                readonly property real fade: firstGlyph ? firstGlyph.exitAlpha : 1
                x: slot.cx + modelData.x + (modelData.dx || 0)
                y: slot.cy + modelData.y + (modelData.dy || 0)
                width: modelData.w
                height: modelData.h
                visible: slot.laidOut && hanabi.t >= arrive
                opacity: fade
                transform: Scale {
                    origin.x: blk.modelData.kind === "band" || blk.modelData.kind === "marker" ? 0 : blk.width / 2
                    origin.y: blk.height / 2
                    xScale: blk.modelData.kind === "marker"
                            ? H.clamp01(blk.firstGlyph ? (hanabi.t - blk.firstGlyph.sungStart) / Math.max(0.1, blk.firstGlyph.sungDur) : 0)
                            : blk.modelData.kind === "band" ? H.easeOutCubic(H.clamp01((hanabi.t - blk.arrive) / 0.4)) : blk.grow
                    yScale: blk.modelData.kind === "band" || blk.modelData.kind === "marker" ? 1 : blk.grow
                }
                Rectangle {
                    anchors.fill: parent
                    radius: blk.modelData.radius || 0
                    color: blk.modelData.kind === "outline" ? "transparent" : blk.modelData.color
                    border.width: blk.modelData.kind === "outline" ? Math.max(2, slot.fontSize * 0.06) : 0
                    border.color: blk.modelData.color
                    opacity: blk.modelData.kind === "marker" ? 0.85 : 1
                    transform: Matrix4x4 { // slant: a parallelogram
                        matrix: blk.modelData.kind === "slant"
                                ? Qt.matrix4x4(1, -0.32, 0, blk.height * 0.16, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
                                : Qt.matrix4x4()
                    }
                }
            }
        }

        // Glyphs.
        Repeater {
            id: glyphRepeater
            model: slot.glyphs
            onCountChanged: Qt.callLater(slot.relayout)
            delegate: Item {
                id: glyph
                required property var modelData
                required property int index
                readonly property real glyphWidth: body.implicitWidth
                readonly property var p: slot.placed[index] ?? { x: 0, y: 0, rot: 0 }
                readonly property real sungStart: modelData.sungStart
                readonly property real sungDur: modelData.sungEnd - modelData.sungStart
                readonly property var sched: slot.shot ? slot.shot.schedule : { start: 0, duration: 0.6, stagger: 0.04 }
                readonly property real enterAt: sched.start + index * sched.stagger
                readonly property real arrival: enterAt + (H.ENTER_ARRIVAL[slot.shot ? slot.shot.enter : "pop"] ?? 0) * sched.duration
                readonly property real exitAt: slot.exitStart + index * 0.02
                readonly property real r0: H.hash01(index + 1, slot.lineIndex * 131 + 7)
                readonly property real r1: H.hash01(index + 2, slot.lineIndex * 131 + 9)
                readonly property real r2: H.hash01(index + 3, slot.lineIndex * 131 + 11)
                readonly property var ctx: ({ r0: r0, r1: r1, r2: r2, motion: hanabi.motion,
                                              toCenterX: -p.x / Math.max(1, slot.fontSize), toCenterY: -p.y / Math.max(1, slot.fontSize),
                                              dirX: 1, dirY: 0, time: hanabi.t - enterAt })
                readonly property var enterPose: slot.shot ? H.enterPose(slot.shot.enter, (hanabi.t - enterAt) / sched.duration, ctx) : H.rest()
                readonly property var exitPose: slot.shot ? H.exitPose(slot.shot.exit, (hanabi.t - exitAt) / H.EXIT_DURATION, ctx) : H.rest()
                readonly property var sing: slot.shot ? H.singPose(slot.shot.sing, hanabi.t - sungStart, sungDur, r0, hanabi.motion)
                                                      : H.singPose("none", -1, 0, 0, 1)
                readonly property real exitAlpha: exitPose.alpha
                readonly property bool started: hanabi.t >= enterAt
                readonly property color restColor: slot.shot ? (slot.shot.block === "none" || slot.shot.block === "outline"
                                                                 || slot.shot.block === "marker"
                                                                 ? slot.shot.palette.paper : slot.shot.palette.ink) : "white"

                x: slot.cx + p.x - width / 2
                y: slot.cy + p.y - height / 2
                width: body.implicitWidth
                height: body.implicitHeight
                visible: slot.laidOut && started && exitPose.alpha > 0.003
                opacity: enterPose.alpha * exitPose.alpha * enterPose.reveal

                transform: [
                    Scale {
                        origin.x: glyph.width / 2
                        origin.y: glyph.height / 2
                        xScale: glyph.enterPose.sx * glyph.exitPose.sx * glyph.sing.sx * glyph.sing.scale
                        yScale: glyph.enterPose.sy * glyph.exitPose.sy * glyph.sing.sy * glyph.sing.scale
                    },
                    Rotation {
                        origin.x: glyph.width / 2
                        origin.y: glyph.height / 2
                        angle: (glyph.p.rot + glyph.enterPose.rot + glyph.exitPose.rot + glyph.sing.rot) * 57.2958
                    },
                    Translate {
                        x: (glyph.enterPose.dx + glyph.exitPose.dx + glyph.sing.dx) * slot.fontSize
                        y: (glyph.enterPose.dy + glyph.exitPose.dy + glyph.sing.dy) * slot.fontSize
                    }
                ]

                // Glow when sung.
                Text {
                    anchors.fill: parent
                    visible: glyph.sing.glow > 0.02
                    text: body.text
                    font: body.font
                    color: slot.shot ? slot.shot.palette.glow : "white"
                    opacity: glyph.sing.glow
                    layer.enabled: visible
                    layer.effect: MultiEffect { blurEnabled: true; blur: 0.8; blurMax: 28; brightness: 0.25; autoPaddingEnabled: true }
                }
                // Hard shadow (Japanese PV style).
                Text {
                    x: slot.fontSize * 0.07
                    y: slot.fontSize * 0.07
                    visible: slot.shot ? slot.shot.hardShadow : false
                    text: body.text
                    font: body.font
                    color: "#2b1f3d"
                    opacity: 0.8
                }
                Text {
                    id: body
                    text: glyph.modelData.ch
                    font.family: hanabi.theme.fontFamily
                    font.weight: Font.Black
                    font.pixelSize: Math.max(8, Math.round(slot.fontSize))
                    style: slot.shot && slot.shot.outline ? Text.Outline : Text.Normal
                    styleColor: "#1b1030"
                    readonly property color tinted: glyph.enterPose.backside
                                                    ? (slot.shot ? slot.shot.palette.blocks[0] : "white")
                                                    : Qt.tint(glyph.restColor, Qt.rgba(1, 0.9, 0.6, glyph.sing.color * 0.7))
                    color: Qt.tint(tinted, Qt.rgba(1, 1, 1, Math.min(1, glyph.sing.flash)))
                    // zoom / dissolve: blurred while far / fading.
                    layer.enabled: glyph.enterPose.blur + glyph.exitPose.blur > 0.05
                    layer.effect: MultiEffect {
                        blurEnabled: true
                        blur: Math.min(1, glyph.enterPose.blur + glyph.exitPose.blur)
                        blurMax: 24
                        autoPaddingEnabled: true
                    }
                }
                // Underline (sing = underline).
                Rectangle {
                    visible: glyph.sing.underline > 0
                    anchors.top: parent.bottom
                    anchors.topMargin: -slot.fontSize * 0.1
                    width: parent.width * glyph.sing.underline
                    height: Math.max(2, slot.fontSize * 0.07)
                    color: slot.shot ? slot.shot.palette.blocks[0] : "white"
                }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            y: slot.cy + hanabi.height * 0.27
            width: Math.min(implicitWidth, hanabi.width * 0.8)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: hanabi.showTranslation && (slot.line.translation ?? "").length > 0 && slot.laidOut
            text: slot.line.translation ?? ""
            color: "#fffdf7"
            style: Text.Outline
            styleColor: "#40000000"
            opacity: 0.85 * H.clamp01((hanabi.t - (slot.line.startMs ?? 0) / 1000) / 0.5)
            font.family: hanabi.theme.fontFamily
            font.weight: Font.DemiBold
            font.pixelSize: Math.round(Math.max(16, hanabi.shortSide * 0.032))
        }

        InstrumentalDots {
            anchors.horizontalCenter: parent.horizontalCenter
            y: hanabi.height * 0.78
            visible: slot.isInstrumental
            dotSize: hanabi.shortSide * 0.022
            color: "#fffdf7"
            active: slot.lineIndex === hanabi.cursor.activeIndex
            progress: active ? hanabi.cursor.progress : 0
            positionMs: hanabi.positionMs
        }
    }

    // Fireworks for a shot, once its glyphs are placed.
    function scheduleShot(slot) {
        const index = slot.lineIndex, shot = slot.shot, line = slot.line
        if (!shot)
            return
        sparks.removeGroup(index)
        const start = line.startMs / 1000
        burstAt(shot, index, slot.cx, slot.cy, null, line.isChorus ? 1.15 : 1, start + 0.05)
        if (shot.extra)
            burstAt(shot, index, slot.cx + width * 0.18 * (shot.rand < 0.5 ? -1 : 1), slot.cy - height * 0.08,
                    shot.extra, 1, start + 0.5)
        const colors = shot.palette.sparks
        const glyphs = slot.glyphs, placed = slot.placed
        // Small bursts on sung words (chorus lines and "senrin"-like shots).
        if (shot.sungBurst) {
            let lastUnit = -1, count = 0
            for (let i = 0; i < glyphs.length && count < 6; ++i) {
                if (glyphs[i].unit === lastUnit || glyphs[i].ch.trim() === "")
                    continue
                lastUnit = glyphs[i].unit
                if (glyphs[i].unit % 2 === 1)
                    continue
                const p = placed[i]
                sparks.burst(count % 2 ? "kiku" : "botan", slot.cx + p.x, slot.cy + p.y - slot.fontSize * 1.1,
                             glyphs[i].sungStart, colors, shortSide * 0.075, 1.1, 34, 0.3, 0.4, 1, 0, index * 100 + i, index)
                ++count
            }
        }
        // Sparkler crackle on each glyph as it is sung.
        if (shot.glyphSparks)
            glyphs.forEach((g, i) => {
                if (g.ch.trim() !== "")
                    sparks.sparkle(slot.cx + placed[i].x, slot.cy + placed[i].y, g.sungStart, colors[i % colors.length],
                                   slot.fontSize * 0.5, 8, index * 100 + i, index)
            })
        // Glyphs that dissolve leave sparks behind.
        if (shot.exit === "dissolve")
            glyphs.forEach((g, i) => {
                if (g.ch.trim() !== "")
                    sparks.sparkle(slot.cx + placed[i].x, slot.cy + placed[i].y, slot.exitStart + i * 0.02, colors[i % colors.length],
                                   slot.fontSize * 0.45, 7, index * 1000 + i, index)
            })
    }

    LineShot { id: slotA }
    LineShot { id: slotB }

    TrackBadge {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Math.max(24, hanabi.height * 0.04)
        visible: hanabi.showCoverInfo
        theme: hanabi.theme
        coverSource: hanabi.coverSource
        size: Math.max(48, hanabi.height * 0.065)
    }
}
