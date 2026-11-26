// SPDX-License-Identifier: AGPL-3.0-or-later
// Partita (云阶) helpers. Logic follows Folia (AGPL-3.0, chthollyphile/folia-major):
// src/components/visualizer/partita/partitaLayout.ts (row chunking, stagger) and
// glowWordVariants.ts / PartitaChunk.tsx (spring stiffness 200, damping 20).
.pragma library

// Folia's seeded random: fract(sin(seed++) * 10000).
function rng(seed) {
    var s = seed
    return function () {
        var x = Math.sin(s++) * 10000
        return x - Math.floor(x)
    }
}

// Spring from 0 to 1 at time t (seconds): stiffness 200, damping 20, mass 1 (slightly
// under-damped, ~4% overshoot). As a pure function of time, so seeking shows the right frame.
var OMEGA = Math.sqrt(200)
var ZETA = 20 / (2 * Math.sqrt(200))
var OMEGA_D = OMEGA * Math.sqrt(1 - ZETA * ZETA)
function spring(t) {
    if (t <= 0)
        return 0
    if (t > 1.2)
        return 1
    var e = Math.exp(-ZETA * OMEGA * t)
    return 1 - e * (Math.cos(OMEGA_D * t) + (ZETA * OMEGA / OMEGA_D) * Math.sin(OMEGA_D * t))
}

function clamp01(v) { return v < 0 ? 0 : v > 1 ? 1 : v }
function easeOut(v) { v = clamp01(v); return 1 - (1 - v) * (1 - v) * (1 - v) }

function isCjk(text) {
    return /[぀-ヿ㐀-鿿豈-﫿가-힯]/.test(text)
}

// Split a line's units into rows of deliberately uneven length (Folia: "perfectly even
// splitting looked too mechanical"), alternating a left / right stagger.
// Returns [{units: [...], stagger, scale, passedRotate}].
function buildRows(units, maxRows, seed, staggerMin, staggerMax, motion) {
    var rand = rng(seed)
    var rowCount = Math.max(1, Math.min(units.length, maxRows))
    var rows = []
    var remaining = units.length, chunksLeft = rowCount, index = 0
    for (var c = 0; c < rowCount; ++c) {
        var avg = remaining / chunksLeft
        var len = 1
        if (c === rowCount - 1) {
            len = remaining
        } else {
            var max = Math.ceil(avg * 1.5)
            len = Math.max(1, Math.min(max, Math.round(avg + (rand() - 0.5) * avg)))
        }
        len = Math.max(1, Math.min(len, remaining - (chunksLeft - 1)))
        var left = c % 2 === 0
        var magnitude = staggerMin + rand() * Math.max(staggerMax - staggerMin, 0)
        var calm = motion < 0.5
        rows.push({
            units: units.slice(index, index + len),
            stagger: (left ? -magnitude : magnitude),
            left: left,
            scale: calm ? 1 : 0.85 + rand() * 0.6 * Math.min(1, motion),
            rotate: motion > 1.5 ? (left ? -3 : 3) : 0,
            passedRotate: (left ? 1 : -1) * (motion > 1.5 ? 6 : 3),
        })
        index += len
        remaining -= len
        chunksLeft--
    }
    return rows
}

// Per-word scatter (where a word flies in from) and resting jitter, seeded by its start.
function wordConfig(startMs, idx, motion) {
    var seed = startMs / 1000 + idx * 7.13
    function r(o) { var x = Math.sin(seed + o) * 10000; return x - Math.floor(x) }
    var spread = motion > 1.5 ? 15 : motion < 0.5 ? 0 : 6
    var turn = motion > 1.5 ? 8 : motion < 0.5 ? 0 : 3
    var x = (r(1) - 0.5) * spread * 2
    var y = (r(2) - 0.5) * spread * 2
    return {
        x: x,
        y: y,
        rotate: (r(3) - 0.5) * turn * 2,
        // Folia: waiting x + sin(y) * 100, y + cos(x) * 50 — use seeded angles for the same spread.
        fromX: Math.sin(r(4) * 6.283) * 100,
        fromY: Math.cos(r(5) * 6.283) * 50,
        passedRotate: (r(8) - 0.5) * 20,
        ripple: 1.5 + r(9) * 2,
    }
}
