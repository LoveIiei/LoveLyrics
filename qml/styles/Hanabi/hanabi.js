// SPDX-License-Identifier: AGPL-3.0-or-later
// Hanabi (花火) — glyph poses, shot catalogue, palettes and the per-song program.
//
// Ported from Folia's Hanabi mode (AGPL-3.0, chthollyphile/folia-major,
// src/components/visualizer/hanabi/): text/animations.ts (enter / sing / exit poses, kept
// formula for formula; scramble, split and glitch are not ported), catalog.ts (families and
// recipes, a curated subset), color.ts (palette roles). Displacements are in font-size
// units, rotations in radians, as in the original.
.pragma library

// ─── easing ───────────────────────────────────────────────────────────────────
function clamp01(v) { return v <= 0 ? 0 : v >= 1 ? 1 : v }
function easeOutCubic(t) { return 1 - Math.pow(1 - t, 3) }
function easeInCubic(t) { return t * t * t }
function easeInQuad(t) { return t * t }
function easeOutQuad(t) { return 1 - (1 - t) * (1 - t) }
function easeOutExpo(t) { return t >= 1 ? 1 : 1 - Math.pow(2, -10 * t) }
function easeOutBack(t, s) { if (s === undefined) s = 1.70158; return 1 + (s + 1) * Math.pow(t - 1, 3) + s * Math.pow(t - 1, 2) }
function easeInBack(t, s) { if (s === undefined) s = 1.70158; return (s + 1) * t * t * t - s * t * t }
function easeInOutCubic(t) { return t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2 }
function dampedCos(q, f, d) { return Math.exp(-d * q) * Math.cos(f * q) * (1 - q) }
function dampedSin(q, f, d) { return Math.exp(-d * q) * Math.sin(f * q) * (1 - q) }
function hash01(a, b) {
    var h = Math.imul((a | 0) ^ 0x9e3779b9, 0x85ebca6b) ^ Math.imul(((b | 0) + 0x632be5ab) | 0, 0xc2b2ae35)
    h ^= h >>> 15; h = Math.imul(h, 0x2c1b3c6d); h ^= h >>> 12; h = Math.imul(h, 0x297a2d39); h ^= h >>> 15
    return (h >>> 0) / 4294967296
}

// Folia's mulberry-style seeded random.
function rng(seed) {
    var a = seed >>> 0
    return function () {
        a = (a + 0x6D2B79F5) | 0
        var t = Math.imul(a ^ (a >>> 15), 1 | a)
        t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296
    }
}

// ─── poses ────────────────────────────────────────────────────────────────────
var ENTER_DURATION = { pop: 0.45, drop: 0.62, rise: 0.6, spin: 0.62, wipe: 0.42, type: 0.12, stretch: 0.55,
    elastic: 0.85, spiral: 0.85, flip: 0.55, stamp: 0.5, wave: 0.75, zoom: 0.6, burst: 0.75 }
var ENTER_ARRIVAL = { pop: 0, drop: 0.4, rise: 0.3, spin: 0.2, wipe: 0, type: 0, stretch: 0.1, elastic: 0.1,
    spiral: 0.45, flip: 0.15, stamp: 0.35, wave: 0.3, zoom: 0.25, burst: 0.3 }
var EXIT_DURATION = 0.6

function rest() { return { dx: 0, dy: 0, sx: 1, sy: 1, rot: 0, alpha: 1, skew: 0, blur: 0, reveal: 1, backside: false } }

function motionScale(p, m) {
    if (m === 1) return p
    p.dx *= m; p.dy *= m; p.rot *= m; p.skew *= m
    p.sx = 1 + (p.sx - 1) * m; p.sy = 1 + (p.sy - 1) * m
    return p
}

// ctx: {r0, r1, r2, toCenterX, toCenterY, dirX, dirY, time, motion}
function enterPose(fx, p, ctx) {
    var pose = rest()
    if (p >= 1) return pose
    var q = clamp01(p)
    var e, f, g, k
    switch (fx) {
    case 'pop':
        var s = easeOutBack(q, 2.4)
        pose.sy = s * (1 + 0.14 * dampedSin(q, 14, 3)); pose.sx = s * (1 - 0.1 * dampedSin(q, 14, 3))
        pose.rot = (ctx.r0 - 0.5) * 0.7 * (1 - easeOutCubic(q)); pose.dy = 0.25 * (1 - easeOutCubic(q))
        pose.alpha = clamp01(q * 6); break
    case 'drop':
        if (q < 0.48) { f = q / 0.48; pose.dy = -2.4 * (1 - f * f); pose.sy = 1 + 0.3 * f; pose.sx = 1 - 0.12 * f }
        else { f = (q - 0.48) / 0.52; pose.dy = -0.32 * Math.abs(Math.sin(f * Math.PI * 2)) * Math.exp(-3 * f) * (1 - f)
               pose.sy = 1 - 0.3 * dampedCos(f, 16, 4); pose.sx = 1 + 0.22 * dampedCos(f, 16, 4) }
        pose.alpha = clamp01(q * 7); break
    case 'rise':
        f = clamp01(q / 0.58); e = easeOutCubic(f); pose.dy = 1.7 * (1 - e)
        if (q < 0.58) { pose.sy = 1 + 0.55 * (1 - e); pose.sx = 1 - 0.28 * (1 - e) }
        else { g = (q - 0.58) / 0.42; pose.sy = 1 - 0.2 * dampedSin(g, 11, 2.5); pose.sx = 1 + 0.16 * dampedSin(g, 11, 2.5)
               pose.dy += 0.08 * dampedSin(g, 11, 2.5) }
        pose.alpha = clamp01(q * 4); break
    case 'spin':
        e = easeOutBack(q, 1.9); pose.rot = -2.6 * (1 - e) * (ctx.r0 < 0.5 ? 1 : -1)
        pose.sx = pose.sy = 0.15 + 0.85 * e
        pose.dx = -0.7 * (1 - easeOutCubic(q)) * ctx.dirX
        pose.dy = -0.7 * (1 - easeOutCubic(q)) * ctx.dirY - 0.3 * Math.sin(Math.PI * q) * (1 - q)
        pose.alpha = clamp01(q * 4); break
    case 'wipe':
        e = easeOutCubic(q); pose.reveal = easeOutCubic(clamp01(q / 0.8))
        pose.dx = -0.35 * (1 - e) * ctx.dirX; pose.dy = -0.35 * (1 - e) * ctx.dirY; pose.skew = -0.3 * (1 - e)
        pose.alpha = clamp01(q * 8); break
    case 'type':
        pose.alpha = q > 0 ? 1 : 0; pose.sx = pose.sy = 1 + 0.3 * Math.pow(1 - clamp01(q / 0.6), 2); break
    case 'stretch':
        k = Math.pow(1 - q, 3)
        pose.sx = 1 + 2 * k - 0.16 * dampedSin(q, 13, 3); pose.sy = 1 - 0.72 * k + 0.12 * dampedSin(q, 13, 3)
        pose.dx = -1.4 * k * ctx.dirX; pose.dy = -1.4 * k * ctx.dirY; pose.alpha = clamp01(q * 5); break
    case 'elastic':
        var angle = ctx.r0 * Math.PI * 2
        var spr = Math.exp(-4.2 * q) * Math.cos(q * Math.PI * 4.5) * (1 - q)
        pose.dx = Math.cos(angle) * 1.6 * spr; pose.dy = Math.sin(angle) * 1.6 * spr
        pose.rot = (ctx.r1 - 0.5) * 0.8 * spr; pose.sx = 1 + 0.18 * Math.abs(spr); pose.sy = 1 - 0.12 * Math.abs(spr)
        pose.alpha = clamp01(q * 5); break
    case 'spiral':
        e = easeOutCubic(q); var radius = 3.4 * (1 - e); var a2 = ctx.r0 * Math.PI * 2 + Math.PI * 2.5 * (1 - e)
        pose.dx = Math.cos(a2) * radius; pose.dy = Math.sin(a2) * radius * 0.8
        pose.rot = -Math.PI * 2 * (1 - e) * (ctx.r1 < 0.5 ? 1 : -1)
        pose.sx = pose.sy = 0.25 + 0.75 * easeOutBack(q, 1.5); pose.alpha = clamp01(q * 3); break
    case 'flip':
        var phi = Math.PI * 2.5 * (1 - easeOutCubic(q))
        pose.sx = Math.cos(phi); pose.backside = pose.sx < 0; pose.sy = 1 + 0.08 * Math.sin(phi)
        pose.dy = -0.3 * Math.sin(Math.PI * q) * (1 - q); pose.alpha = clamp01(q * 6); break
    case 'stamp':
        if (q < 0.5) { f = q / 0.5; pose.sx = pose.sy = 2.6 - 1.6 * easeInQuad(f)
                       pose.rot = (ctx.r0 - 0.5) * 0.35 * (1 - f); pose.alpha = clamp01(f * 2.2) }
        else { f = (q - 0.5) / 0.5; pose.sx = 1 + 0.22 * dampedCos(f, 17, 4); pose.sy = 1 - 0.22 * dampedCos(f, 17, 4) }
        break
    case 'wave':
        k = Math.pow(1 - q, 1.5)
        pose.dy = 1.1 * k * Math.cos(q * Math.PI * 3); pose.rot = 0.4 * k * Math.sin(q * Math.PI * 3)
        pose.sy = 1 + 0.18 * k * Math.sin(q * Math.PI * 3 + 0.6); pose.alpha = clamp01(q * 4); break
    case 'zoom':
        e = easeOutExpo(q); k = 1.8 * (1 - e); pose.sx = pose.sy = 1 + k * 1.5
        pose.dx = -ctx.toCenterX * k; pose.dy = -ctx.toCenterY * k; pose.blur = Math.pow(1 - q, 1.4)
        pose.alpha = clamp01(q * 3.5); break
    case 'burst':
    default:
        e = easeOutBack(q, 1.8); pose.dx = ctx.toCenterX * (1 - e); pose.dy = ctx.toCenterY * (1 - e)
        pose.sx = pose.sy = 0.2 + 0.8 * easeOutBack(clamp01(q * 1.3), 1.6)
        pose.rot = (ctx.r2 - 0.5) * 4 * (1 - easeOutCubic(q)); pose.alpha = clamp01(q * 6); break
    }
    return motionScale(pose, ctx.motion)
}

function exitPose(fx, p, ctx) {
    var pose = rest()
    if (p <= 0) return pose
    var q = clamp01(p), e
    switch (fx) {
    case 'ascend':
        e = easeInCubic(q); var dip = q < 0.22 ? Math.sin((q / 0.22) * Math.PI) * 0.12 : 0
        pose.dy = dip - 2.8 * e; pose.dx = (ctx.r0 - 0.5) * 0.7 * e; pose.rot = (ctx.r1 - 0.5) * 0.5 * e
        pose.sy = 1 + 0.45 * e; pose.sx = 1 - 0.22 * e; pose.alpha = 1 - easeInQuad(q); break
    case 'scatter':
        var ox = -ctx.toCenterX + (ctx.r0 - 0.5) * 2, oy = -ctx.toCenterY + (ctx.r1 - 0.5) * 2
        var len = Math.hypot(ox, oy) || 1; ox /= len; oy /= len
        e = easeOutCubic(q); var dist = 2.4 + ctx.r2 * 1.8
        pose.dx = ox * dist * e; pose.dy = oy * dist * e; pose.rot = (ctx.r1 - 0.5) * 3.2 * e
        pose.sx = pose.sy = 1 - 0.65 * e; pose.alpha = 1 - Math.pow(q, 1.6); break
    case 'shrink':
        var s = q < 0.2 ? 1 + 0.16 * Math.sin((q / 0.2) * Math.PI / 2) : 1.16 * (1 - easeInBack((q - 0.2) / 0.8, 1.4))
        pose.sx = pose.sy = Math.max(0, s); pose.rot = (ctx.r0 - 0.5) * 0.9 * q
        pose.alpha = q > 0.8 ? (1 - q) / 0.2 : 1; break
    case 'slide':
        e = easeInBack(q, 1.6)
        pose.dx = ctx.dirX * 7 * e; pose.dy = ctx.dirY * 7 * e
        pose.sx = 1 + 0.45 * easeInQuad(q) * Math.abs(ctx.dirX); pose.sy = 1 + 0.45 * easeInQuad(q) * Math.abs(ctx.dirY)
        pose.skew = -0.45 * easeInQuad(q) * (ctx.dirX < 0 ? -1 : 1)
        pose.alpha = 1 - Math.pow(clamp01((q - 0.45) / 0.55), 1.5); break
    case 'dissolve':
        var flicker = hash01(Math.floor(ctx.time * 20), Math.floor(ctx.r0 * 1e6))
        pose.alpha = Math.pow(1 - q, 1.3) * (1 - 0.45 * q * flicker)
        pose.sx = pose.sy = 1 + 0.14 * easeOutQuad(q); pose.dy = -0.35 * easeOutQuad(q); pose.blur = q; break
    case 'fall':
        e = easeInQuad(q); pose.dy = 3 * e; pose.dx = 0.28 * Math.sin(q * Math.PI * 2 + ctx.r0 * 6) * q
        pose.rot = (ctx.r1 - 0.5) * 1.2 * e; pose.sy = 1 + 0.4 * e; pose.sx = 1 - 0.25 * e
        pose.alpha = 1 - Math.pow(q, 1.6); break
    case 'blink':
        var on = q < 0.14 || (q >= 0.24 && q < 0.4) || (q >= 0.5 && q < 0.6) || (q >= 0.7 && q < 0.76) || (q >= 0.82 && q < 0.85)
        pose.alpha = on && q < 1 ? 1 : 0; pose.sx = pose.sy = 1 + (on ? 0.05 * q : 0); break
    }
    if (q >= 1) pose.alpha = 0
    return motionScale(pose, ctx.motion)
}

// u = seconds since the glyph was sung (< 0: not yet); duration = how long it is sung.
function singPose(fx, u, duration, rand, motion) {
    var pose = { dx: 0, dy: 0, scale: 1, sx: 1, sy: 1, rot: 0, flash: 0, glow: 0, color: 0, underline: 0 }
    if (u < 0 || fx === 'none') return pose
    var hold = Math.max(duration, 0.12)
    var envelope = clamp01(u / 0.08) * (1 - clamp01((u - hold) / 0.35))
    var T, f, land
    switch (fx) {
    case 'bounce':
        T = 0.3
        if (u < T) pose.dy = -0.26 * Math.sin((u / T) * Math.PI)
        else if (u < T + 0.16) pose.dy = -0.06 * Math.sin(((u - T) / 0.16) * Math.PI)
        land = u - T
        if (land >= 0 && land < 0.3) { f = land / 0.3; pose.sy = 1 - 0.14 * dampedCos(f, 14, 3); pose.sx = 1 + 0.1 * dampedCos(f, 14, 3) }
        else if (u < T) pose.sy = 1 + 0.08 * Math.sin((u / T) * Math.PI)
        break
    case 'flash':
        pose.flash = Math.exp(-u / 0.16); pose.scale = 1 + 0.2 * Math.exp(-u / 0.1) * clamp01(u / 0.03); break
    case 'swell':
        pose.scale = 1 + 0.24 * easeOutBack(clamp01(u / 0.18), 2) * (1 - clamp01((u - hold) / 0.35)); break
    case 'shake':
        var k = clamp01(u / 0.04) * Math.exp(-Math.max(0, u - hold) / 0.08) * (u < hold + 0.4 ? 1 : 0)
        pose.dx = 0.06 * Math.sin(u * 61 + rand * 9) * k; pose.dy = 0.04 * Math.sin(u * 47 + rand * 5) * k
        pose.rot = 0.08 * Math.sin(u * 53 + rand * 7) * k; break
    case 'jump':
        T = 0.42
        if (u < T) { f = u / T; pose.dy = -0.5 * Math.sin(f * Math.PI)
                     pose.rot = (rand < 0.5 ? 1 : -1) * 0.22 * Math.sin(f * Math.PI * 2) * (1 - f)
                     pose.sy = 1 + 0.2 * Math.sin(f * Math.PI) * (1 - f); pose.sx = 1 - 0.12 * Math.sin(f * Math.PI) * (1 - f) }
        else if (u < T + 0.35) { f = (u - T) / 0.35; pose.sy = 1 - 0.2 * dampedCos(f, 13, 3); pose.sx = 1 + 0.16 * dampedCos(f, 13, 3) }
        break
    case 'glow':
        pose.glow = envelope; pose.flash = 0.35 * envelope; pose.scale = 1 + 0.07 * envelope; break
    case 'underline':
        pose.dy = -0.05 * envelope; pose.underline = clamp01(u / Math.max(0.15, hold)); break
    case 'color':
        pose.color = clamp01(u / 0.1); pose.scale = 1 + 0.14 * Math.exp(-u / 0.12) * clamp01(u / 0.03); break
    }
    if (motion !== 1) {
        pose.dx *= motion; pose.dy *= motion; pose.rot *= motion
        pose.scale = 1 + (pose.scale - 1) * motion; pose.sx = 1 + (pose.sx - 1) * motion; pose.sy = 1 + (pose.sy - 1) * motion
    }
    return pose
}

// ─── palettes (Folia color.ts roles; "cover" is ours: from the album art) ─────────────
var PALETTES = {
    kawaii:   { blocks: ['#ffb3c6', '#bde0fe', '#fcf6bd'], ink: '#2b1f3d', paper: '#fffdf7', sparks: ['#ff8fab', '#a2d2ff', '#ffe066', '#ffffff'], glow: '#ffc8dd' },
    festival: { blocks: ['#ff4d6d', '#ffd166', '#06d6a0'], ink: '#2b1f3d', paper: '#fffdf7', sparks: ['#ff4d6d', '#ffd166', '#ffffff', '#7bdff2'], glow: '#ffd166' },
    ocean:    { blocks: ['#4cc9f0', '#caf0f8', '#4895ef'], ink: '#10223d', paper: '#f4fbff', sparks: ['#4cc9f0', '#90e0ef', '#ffffff', '#4361ee'], glow: '#90e0ef' },
    gold:     { blocks: ['#f7c873', '#fff1c9', '#e9a23b'], ink: '#2b1f3d', paper: '#fffaf0', sparks: ['#ffd36b', '#fff1c9', '#ff9f43', '#ffffff'], glow: '#ffd36b' },
    neon:     { blocks: ['#f72585', '#4cc9f0', '#7209b7'], ink: '#ffffff', paper: '#ffffff', sparks: ['#f72585', '#4cc9f0', '#ffffff', '#b5179e'], glow: '#f72585' },
    night:    { blocks: ['#e0e7ff', '#a5b4fc', '#c7d2fe'], ink: '#1e1b4b', paper: '#f8fafc', sparks: ['#ffffff', '#a5b4fc', '#fde68a', '#93c5fd'], glow: '#c7d2fe' },
}

function coverPalette(theme) {
    var s = function (c) { return c.toString() }
    return { blocks: [s(theme.vibrant), s(theme.light), s(theme.accent)], ink: '#2b1f3d', paper: '#fffdf7',
             sparks: [s(theme.vibrant), s(theme.accent), s(theme.light), '#ffffff'], glow: s(theme.accent) }
}

// ─── catalogue (curated from Folia's 10 families × 5) ─────────────────────────────
// burst: [kind, radius (× short side), duration s, count]; at: behind | above | sides | sky | none
function B(kind, radius, duration, count) { return { kind: kind, radius: radius, duration: duration, count: count } }

var FAMILIES = {
    uchiage: { size: 0.1, stagger: 0.045, outline: false, hardShadow: true, launch: true, recipes: [
        { layout: 'center', block: 'band', enter: 'rise', sing: 'bounce', exit: 'ascend', palette: 'kawaii', burst: B('kiku', 0.3, 1.9, 96), at: 'above' },
        { layout: 'stagger', block: 'pill', enter: 'rise', sing: 'jump', exit: 'ascend', palette: 'festival', burst: B('botan', 0.26, 1.4, 96), at: 'above', sungBurst: true },
        { layout: 'left', block: 'slant', enter: 'elastic', sing: 'swell', exit: 'ascend', palette: 'ocean', burst: B('palm', 0.32, 2.3, 60), at: 'sides' },
    ] },
    kiku: { size: 0.13, stagger: 0.035, outline: true, hardShadow: false, launch: true, recipes: [
        { layout: 'center', block: 'none', enter: 'zoom', sing: 'glow', exit: 'shrink', palette: 'night', burst: B('kiku', 0.36, 1.9, 120), at: 'behind' },
        { layout: 'center', block: 'circle', enter: 'pop', sing: 'bounce', exit: 'scatter', palette: 'festival', burst: B('kiku', 0.3, 1.9, 96), at: 'behind', outline: false, hardShadow: true, size: 0.11 },
        { layout: 'arc', block: 'tile', enter: 'spiral', sing: 'swell', exit: 'ascend', palette: 'gold', burst: B('kamuro', 0.32, 3.4, 110), at: 'above', outline: false, size: 0.1 },
        { layout: 'right', block: 'marker', enter: 'wipe', sing: 'underline', exit: 'slide', palette: 'ocean', burst: B('kiku', 0.28, 1.9, 90), at: 'sky', outline: false, size: 0.1 },
    ] },
    botan: { size: 0.11, stagger: 0.05, outline: false, hardShadow: true, launch: false, recipes: [
        { layout: 'center', block: 'band', enter: 'stamp', sing: 'jump', exit: 'shrink', palette: 'festival', burst: B('botan', 0.3, 1.4, 110), at: 'behind', size: 0.12 },
        { layout: 'stagger', block: 'tile', enter: 'pop', sing: 'bounce', exit: 'scatter', palette: 'kawaii', burst: B('botan', 0.26, 1.4, 84), at: 'behind' },
        { layout: 'center', block: 'double', enter: 'drop', sing: 'bounce', exit: 'scatter', palette: 'gold', burst: B('ring', 0.28, 1.6, 72), at: 'behind' },
    ] },
    yanagi: { size: 0.09, stagger: 0.07, outline: false, hardShadow: false, launch: true, recipes: [
        { layout: 'vertical', block: 'none', enter: 'wave', sing: 'glow', exit: 'fall', palette: 'gold', burst: B('yanagi', 0.3, 3.2, 70), at: 'above' },
        { layout: 'center', block: 'none', enter: 'type', sing: 'color', exit: 'fall', palette: 'night', burst: B('yanagi', 0.28, 3.2, 64), at: 'sky' },
    ] },
    senrin: { size: 0.095, stagger: 0.03, outline: false, hardShadow: true, launch: false, recipes: [
        { layout: 'stagger', block: 'circle', enter: 'burst', sing: 'flash', exit: 'dissolve', palette: 'festival', burst: B('senrin', 0.34, 1.9, 90), at: 'behind', sungBurst: true },
        { layout: 'diagonal', block: 'tile', enter: 'pop', sing: 'jump', exit: 'scatter', palette: 'neon', burst: B('senrin', 0.3, 1.8, 84), at: 'sky', sungBurst: true },
    ] },
    hoshikuzu: { size: 0.1, stagger: 0.045, outline: true, hardShadow: false, launch: false, recipes: [
        { layout: 'arc', block: 'none', enter: 'zoom', sing: 'glow', exit: 'dissolve', palette: 'night', burst: B('strobe', 0.3, 2.3, 72), at: 'sky' },
        { layout: 'center', block: 'outline', enter: 'flip', sing: 'flash', exit: 'dissolve', palette: 'neon', burst: B('star', 0.24, 1.6, 60), at: 'above' },
    ] },
    obi: { size: 0.095, stagger: 0.04, outline: false, hardShadow: false, launch: false, recipes: [
        { layout: 'diagonal', block: 'slant', enter: 'stretch', sing: 'bounce', exit: 'slide', palette: 'neon', burst: B('ring', 0.26, 1.4, 60), at: 'sides' },
        { layout: 'stagger', block: 'stair', enter: 'drop', sing: 'jump', exit: 'slide', palette: 'kawaii', burst: B('heart', 0.22, 1.6, 60), at: 'above' },
        { layout: 'left', block: 'band', enter: 'wipe', sing: 'underline', exit: 'slide', palette: 'ocean', burst: B('ring', 0.24, 1.4, 60), at: 'sky' },
    ] },
    shakudama: { size: 0.15, stagger: 0.06, outline: false, hardShadow: true, launch: true, recipes: [
        { layout: 'center', block: 'double', enter: 'stamp', sing: 'swell', exit: 'shrink', palette: 'gold', burst: B('kamuro', 0.46, 3.6, 150), at: 'behind', extra: B('kiku', 0.3, 1.9, 100) },
        { layout: 'center', block: 'none', enter: 'zoom', sing: 'glow', exit: 'dissolve', palette: 'festival', burst: B('kiku', 0.44, 1.9, 150), at: 'behind', outline: true, extra: B('botan', 0.26, 1.4, 90) },
    ] },
    senkohanabi: { size: 0.075, stagger: 0.06, outline: false, hardShadow: false, launch: false, recipes: [
        { layout: 'center', block: 'none', enter: 'type', sing: 'glow', exit: 'dissolve', palette: 'gold', burst: null, at: 'none', glyphSparks: true },
        { layout: 'left', block: 'none', enter: 'wave', sing: 'color', exit: 'dissolve', palette: 'night', burst: null, at: 'none', glyphSparks: true },
    ] },
}

var FAMILIES_BY_KIND = {
    chorus: ['kiku', 'botan', 'senrin', 'shakudama', 'uchiage'],
    lift: ['obi', 'senrin', 'uchiage', 'botan'],
    verse: ['uchiage', 'obi', 'hoshikuzu', 'kiku', 'botan'],
    breath: ['yanagi', 'senkohanabi', 'hoshikuzu'],
    outro: ['yanagi', 'senkohanabi', 'hoshikuzu'],
    break: ['uchiage'],
}

function isCjk(text) { return /[぀-ヿ㐀-鿿豈-﫿가-힯]/.test(text) }

// Shot for one line: family chosen per section (seeded by the song), recipe varies by line.
// The 20%-ish loudest chorus sections may become shakudama (the biggest shells).
function shotFor(seed, line, lineIndex, coverWeight, theme) {
    var kind = line.sectionKind || 'verse'
    var section = line.section >= 0 ? line.section : 0
    var r = rng((seed ^ Math.imul(section + 1, 0x9e3779b1)) >>> 0)
    var pool = FAMILIES_BY_KIND[kind] || FAMILIES_BY_KIND.verse
    var familyName = pool[Math.floor(r() * pool.length) % pool.length]
    var fam = FAMILIES[familyName]
    var lr = rng((seed ^ Math.imul(lineIndex + 7, 0x85ebca6b)) >>> 0)
    var recipe = fam.recipes[(Math.floor(r() * 97) + lineIndex) % fam.recipes.length]
    var layout = recipe.layout
    if (layout === 'vertical' && !isCjk(line.text || ''))
        layout = 'center'
    var paletteName = lr() < coverWeight ? 'cover' : recipe.palette
    var palette = paletteName === 'cover' ? coverPalette(theme) : PALETTES[recipe.palette]
    return {
        family: familyName,
        layout: layout,
        block: recipe.block,
        enter: recipe.enter,
        sing: recipe.sing,
        exit: recipe.exit,
        palette: palette,
        burst: recipe.burst,
        extra: recipe.extra || null,
        at: recipe.at,
        sungBurst: recipe.sungBurst === true || line.isChorus === true,
        glyphSparks: recipe.glyphSparks === true,
        launch: fam.launch,
        size: recipe.size || fam.size,
        stagger: fam.stagger,
        outline: recipe.outline !== undefined ? recipe.outline : fam.outline,
        hardShadow: recipe.hardShadow !== undefined ? recipe.hardShadow : fam.hardShadow,
        swing: (lr() < 0.5 ? -1 : 1) * (6 + lr() * 8),   // degrees, the shot swings in by this
        rand: lr(),
    }
}

// Entrance timing: every glyph has landed by the time the first one is sung.
function enterSchedule(shot, glyphCount, lineStartS) {
    var dur = ENTER_DURATION[shot.enter] || 0.6
    var stagger = shot.stagger
    var lead = dur + stagger * Math.max(0, glyphCount - 1)
    if (lead > 1.6) {
        stagger = Math.max(0.008, (1.6 - dur) / Math.max(1, glyphCount - 1))
        lead = dur + stagger * Math.max(0, glyphCount - 1)
    }
    return { start: lineStartS - lead, duration: dur, stagger: stagger }
}

function graphemes(text) { return Array.from(text) }
