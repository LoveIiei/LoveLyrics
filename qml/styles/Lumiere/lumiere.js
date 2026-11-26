// SPDX-License-Identifier: AGPL-3.0-or-later
// Lumiere (绘光) — light rigs, the CPU beam mask (the same formula as shaders/lightfield.frag,
// so a glyph is as bright as the light that falls on it), palette and per-section program.
//
// Ported from Folia's Lumiere (AGPL-3.0, chthollyphile/folia-major,
// src/components/visualizer/lumiere/): light/rig.ts (beam model, beamMask, compression),
// rigs/base.ts (shaft / haze / glare defaults), rigs/stage.ts (the stage family of rigs),
// color.ts (champagne light). Units: height units, y down; angle 0 = right, π/2 = down.
.pragma library

var DOWN = Math.PI / 2

function smoothstep(e0, e1, x) { var t = Math.min(1, Math.max(0, (x - e0) / (e1 - e0))); return t * t * (3 - 2 * t) }
function clamp01(v) { return v < 0 ? 0 : v > 1 ? 1 : v }
function fract(x) { return x - Math.floor(x) }
function hash11(p) { p = fract(p * 0.1031); p *= p + 33.33; p *= p + p; return fract(p) }
function valueNoise1(x) { var i = Math.floor(x), f = x - i, u = f * f * (3 - 2 * f); return hash11(i) * (1 - u) + hash11(i + 1) * u }

function merge(a, b) { var o = {}; for (var k in a) o[k] = a[k]; for (var k2 in b) o[k2] = b[k2]; return o }

// ─── building blocks (rigs/base.ts) ────────────────────────────────────────────────
function shaft(o) {
    return merge({ x: 0.5, y: -0.04, angle: DOWN, spread: 0.09, width: 0.05, length: 0.8, softness: 0.6,
                   intensity: 0.95, streaks: 0.6, streakFreq: 7, streakSpeed: 0.08, core: 0.75,
                   sway: { amplitude: 0.012, period: 13, phase: 0 } }, o || {})
}
var FOG = { density: 1, tyndallBase: 0.12, scale: 2.4, driftX: 0.008, driftY: -0.04, warp: 0.9, ambient: 0.03 }
var HAZE = merge(FOG, { density: 1.25, tyndallBase: 0.08, scale: 2, driftX: 0.01, driftY: -0.015, warp: 1.1, ambient: 0.035 })

// A lamp at a pointing at b (screen fractions).
function lamp(aspect, a, b, o) {
    return shaft(merge({ x: a[0], y: a[1], angle: Math.atan2(b[1] - a[1], (b[0] - a[0]) * aspect), spread: 0.08,
                         width: 0.03, length: 1.3, softness: 0.55, intensity: 1, streaks: 0.4, streakFreq: 6,
                         core: 0.6, sway: null }, o || {}))
}
function stageRig(beams, glare, fog) { return { beams: beams, fog: merge(HAZE, fog || {}), glare: glare || null } }
var CENTER = [0.5, 0.56]

// ─── the stage family (rigs/stage.ts) ──────────────────────────────────────────────
// mood: loud / neutral / quiet. region: lyric area (fractions). vertical: prefers vertical CJK.
var PROFILES = [
    { kind: 'spot', label: '独光', mood: 'neutral', motes: 1,
      rig: function (A) { return stageRig([lamp(A, [0.14, 0.02], CENTER, { sway: { amplitude: 0.02, period: 12, phase: 0 } })],
                                          { x: 0.14, y: 0.02, radius: 0.08, intensity: 0.9, streak: 0.5 }) } },
    { kind: 'cross', label: '交叉', mood: 'loud', motes: 1, burst: { density: 0.3, size: 3.4, tint: [1, 0.75, 0.5] },
      rig: function (A) { return stageRig([
          lamp(A, [0.06, 0.02], CENTER, { sway: { amplitude: 0.03, period: 9, phase: 0 } }),
          lamp(A, [0.94, 0.02], CENTER, { sway: { amplitude: 0.03, period: 9, phase: 0.5 } }),
      ], { x: 0.5, y: 0.56, radius: 0.06, intensity: 0.6, streak: 0.6 }) } },
    { kind: 'search', label: '探照', mood: 'loud', motes: 1, region: { cx: 0.5, cy: 0.44, w: 0.72, h: 0.46 },
      rig: function () { return stageRig([0.15, 0.38, 0.62, 0.85].map(function (x, i) { return shaft({
          x: x, y: 1.02, angle: -Math.PI / 2, spread: 0.035, width: 0.03, length: 1.4, softness: 0.5, intensity: 0.8,
          streaks: 0.3, streakFreq: 5, core: 0.6, sway: { amplitude: 0.45, period: 7 + i * 1.3, phase: i * 0.27 } }) })) } },
    { kind: 'footlight', label: '脚光', mood: 'quiet', motes: 0.8, region: { cx: 0.5, cy: 0.46, w: 0.5, h: 0.62 }, vertical: true,
      rig: function () { return stageRig([0.25, 0.5, 0.75].map(function (x) { return shaft({
          x: x, y: 1.03, angle: -Math.PI / 2, spread: 0.22, width: 0.08, length: 0.7, softness: 0.9, intensity: 0.6,
          streaks: 0.4, streakFreq: 10, core: 0.3, tint: [1, 0.8, 0.6], sway: null }) }), null, { driftY: -0.03 }) } },
    { kind: 'backlight', label: '逆光', mood: 'neutral', motes: 1, region: { cx: 0.5, cy: 0.5, w: 0.72, h: 0.5 },
      rig: function () { var b = []; for (var k = 0; k < 6; ++k) b.push(shaft({
          x: 0.5, y: 0.5, angle: (k / 6) * Math.PI * 2 + 0.5, spread: 0.12, width: 0.02, length: 0.8, softness: 0.7,
          intensity: 0.6, streaks: 0.6, streakFreq: 10, core: 0.4, sway: { amplitude: 0.05, period: 11, phase: k / 6 } }))
          return stageRig(b, { x: 0.5, y: 0.5, radius: 0.22, intensity: 1.2, streak: 0.8 }) } },
    { kind: 'pinspot', label: '针光', mood: 'quiet', motes: 0.8,
      rig: function (A) { return stageRig([lamp(A, [0.5, 0.02], CENTER, { spread: 0.03, width: 0.01, intensity: 0.9, core: 0.9 })],
                                          null, { ambient: 0.015 }) } },
    { kind: 'strobe', label: '频闪', mood: 'loud', motes: 1, burst: { density: 0.5, size: 3, tint: [1, 0.9, 0.8] },
      rig: function (A) { return stageRig([0.1, 0.36, 0.64, 0.9].map(function (x, i) { return lamp(A, [x, 0.02], [0.5, 0.6], {
          spread: 0.08, intensity: 0.55, pulse: { amplitude: 0.95, period: 0.5, phase: i * 0.25 } }) }),
          { x: 0.5, y: 0.02, radius: 0.2, intensity: 0.6, streak: 0.4 }) } },
    { kind: 'curtain', label: '幕缝', mood: 'quiet', motes: 0.7, region: { cx: 0.5, cy: 0.5, w: 0.3, h: 0.66 }, vertical: true,
      rig: function () { return stageRig([shaft({ spread: 0.01, width: 0.04, length: 1.6, softness: 0.5, intensity: 0.9,
          streaks: 0.3, core: 0.7, sway: null })], { x: 0.5, y: -0.02, radius: 0.08, intensity: 0.8, streak: 0.5 }, { ambient: 0.02 }) } },
    { kind: 'laser', label: '激光', mood: 'loud', motes: 0.9, region: { cx: 0.5, cy: 0.4, w: 0.72, h: 0.46 },
      rig: function () { var b = []; for (var k = 0; k < 6; ++k) b.push(shaft({
          x: 0.5, y: 1.02, angle: -Math.PI / 2 + (k - 2.5) * 0.22, spread: 0.002, width: 0.004, length: 2, softness: 0.3,
          intensity: 1.2, streaks: 0, core: 0.9, sway: { amplitude: 0.25, period: 4.5, phase: k * 0.08 } }))
          return stageRig(b, { x: 0.5, y: 1.0, radius: 0.06, intensity: 1, streak: 0.6 }, { density: 1.4 }) } },
    { kind: 'projector', label: '放映', mood: 'neutral', motes: 1.8, region: { cx: 0.62, cy: 0.5, w: 0.44, h: 0.58 }, vertical: true,
      rig: function () { return stageRig([shaft({ x: 0.12, y: 0.4, angle: 0.05, spread: 0.2, width: 0.02, length: 1.8,
          softness: 0.4, intensity: 1, streaks: 0.5, streakFreq: 9, core: 0.4, sway: null,
          pulse: { amplitude: 0.05, period: 0.12, phase: 0 } })], { x: 0.12, y: 0.4, radius: 0.05, intensity: 1, streak: 0.4 }) } },
]

var MOODS_BY_KIND = {
    chorus: ['loud'], lift: ['loud', 'neutral'], verse: ['neutral', 'neutral', 'quiet'],
    breath: ['quiet'], outro: ['quiet', 'neutral'], break: ['neutral', 'loud'],
}

function rng(seed) {
    var a = seed >>> 0
    return function () {
        a = (a + 0x6D2B79F5) | 0
        var t = Math.imul(a ^ (a >>> 15), 1 | a)
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296
    }
}

// The rig for a section: a mood that suits it, then a seeded pick within that mood. Two
// sections in a row never share a rig.
function profileFor(seed, sectionIndex, sectionKind, previousKind) {
    var r = rng((seed ^ Math.imul(sectionIndex + 1, 0x9e3779b1)) >>> 0)
    var moods = MOODS_BY_KIND[sectionKind] || MOODS_BY_KIND.verse
    var mood = moods[Math.floor(r() * moods.length) % moods.length]
    var pool = PROFILES.filter(function (p) { return p.mood === mood && p.kind !== previousKind })
    if (pool.length === 0) pool = PROFILES
    return pool[Math.floor(r() * pool.length) % pool.length]
}

// ─── beams at a time (light/rig.ts resolveBeams) ─────────────────────────────────────
function osc(o, t) { return o ? o.amplitude * Math.sin((t / Math.max(o.period, 0.001) + o.phase) * Math.PI * 2) : 0 }

// drive: {intensity, color: [r, g, b], local (seconds since the rig appeared)}
function resolveBeams(rig, t, aspect, drive) {
    var out = []
    for (var i = 0; i < Math.min(6, rig.beams.length); ++i) {
        var b = rig.beams[i]
        var angle = b.angle + osc(b.sway, t)
        var pulse = 1 + osc(b.pulse, t)
        var tint = b.tint || [1, 1, 1]
        var reveal = 0.06 + 0.94 * smoothstep(0, 1, drive.local / 1.2) // beams grow from the lamp
        out.push({
            ox: b.x * aspect, oy: b.y, dx: Math.cos(angle), dy: Math.sin(angle),
            halfWidth: (b.width || 0) / 2, tanSpread: Math.tan(b.spread), softness: b.softness,
            length: b.length * reveal, intensity: b.intensity * pulse * drive.intensity,
            streaks: b.streaks, streakFreq: b.streakFreq, streakPhase: t * b.streakSpeed,
            core: b.core === undefined ? 0.5 : b.core,
            r: tint[0] * drive.color[0], g: tint[1] * drive.color[1], b: tint[2] * drive.color[2],
        })
    }
    return out
}

// Same as beamMask in shaders/lightfield.frag.
function beamMask(beam, x, y) {
    var px = x - beam.ox, py = y - beam.oy
    var along = px * beam.dx + py * beam.dy
    if (along <= 0) return 0
    var side = px * -beam.dy + py * beam.dx
    var half = Math.abs(beam.halfWidth + along * beam.tanSpread) + 1e-4
    var s = Math.abs(side) / half
    var edge = smoothstep(1, 1 - Math.max(beam.softness, 0.02), s)
    if (edge <= 0) return 0
    var signed = s * (side < 0 ? -1 : 1)
    var streak = 1 - beam.streaks + beam.streaks * valueNoise1(signed * beam.streakFreq + beam.streakPhase)
    var core = 1 - beam.core + beam.core * (1 - s * s)
    var falloff = Math.exp(-along / Math.max(beam.length, 0.001))
    var start = smoothstep(0, Math.abs(beam.halfWidth) * 2 + 0.01, along)
    return edge * streak * core * falloff * start * beam.intensity
}

function lightAt(beams, x, y) {
    var sum = 0
    for (var i = 0; i < beams.length; ++i) sum += beamMask(beams[i], x, y)
    return 1 - Math.exp(-sum) // the shader's soft compression
}

// Shader uniforms: four vec4 per beam (see shaders/lightfield.frag).
function packBeam(beam) {
    if (!beam) return [Qt.vector4d(0, 0, 0, 1), Qt.vector4d(0, 0, 0, 1), Qt.vector4d(0, 0, 0, 0), Qt.vector4d(0, 0, 0, 0)]
    return [Qt.vector4d(beam.ox, beam.oy, beam.dx, beam.dy),
            Qt.vector4d(beam.halfWidth, beam.tanSpread, beam.softness, beam.length),
            Qt.vector4d(beam.intensity, beam.streaks, beam.streakFreq, beam.streakPhase),
            Qt.vector4d(beam.r, beam.g, beam.b, beam.core)]
}

// ─── colour (color.ts / scene.ts resolveLumierePalette) ─────────────────────────────
var CHAMPAGNE = [1, 0.86, 0.62]
function lightColor(accent, mix) {
    var a = [accent.r, accent.g, accent.b]
    var base = [0, 1, 2].map(function (k) { return CHAMPAGNE[k] + (a[k] - CHAMPAGNE[k]) * 0.18 })
    // A glowing version of the accent (lifted towards white).
    var peak = Math.max(a[0], a[1], a[2], 0.001)
    var glow = a.map(function (v) { return Math.min(1, v / peak * 0.85 + 0.15) })
    return [0, 1, 2].map(function (k) { return base[k] + (glow[k] - base[k]) * mix })
}
