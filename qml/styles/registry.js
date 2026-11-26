// SPDX-License-Identifier: AGPL-3.0-or-later
// The list of styles. Each entry: name (saved in settings), file (relative to qml/styles),
// a description, and its tunables. A tunable is a slider ({min, max, step}) or a switch
// ({type: "bool"}); `value` is the default. Saved values live in Settings.styleValue().
.pragma library

var styles = [
    {
        name: "Flow",
        file: "Flow/Style.qml",
        description: "Scrolling lyrics, the active line large and sharp",
        tunables: [
            { key: "blur", label: "Blur of other lines", min: 0, max: 2, step: 0.1, value: 1.0 },
            { key: "stagger", label: "Wave delay (ms per line)", min: 0, max: 120, step: 5, value: 45 },
            { key: "anchor", label: "Active line height (from top)", min: 0.2, max: 0.6, step: 0.02, value: 0.36 },
        ],
    },
    {
        name: "Kinetic",
        file: "Kinetic/Style.qml",
        description: "One line at a time, words arriving on their beats",
        tunables: [
            { key: "size", label: "Text size", min: 0.6, max: 1.6, step: 0.05, value: 1.0 },
            { key: "motion", label: "Word motion", min: 0, max: 2, step: 0.1, value: 1.0 },
            { key: "blur", type: "bool", label: "Blur-in words", value: true },
        ],
    },
    {
        name: "Stage",
        file: "Stage/Style.qml",
        description: "Large cover with a three-line lyric window",
        tunables: [
            { key: "tilt", label: "Cover tilt", min: 0, max: 2, step: 0.1, value: 1.0 },
            { key: "reflection", type: "bool", label: "Reflection", value: true },
        ],
    },
    {
        name: "Partita",
        file: "Partita/Style.qml",
        description: "云阶 — rows step left and right, words spring in on their beat (after Folia)",
        tunables: [
            { key: "stagger", label: "Step width", min: 0, max: 180, step: 5, value: 100 },
            { key: "motion", label: "Motion (calm – chaotic)", min: 0, max: 2, step: 0.1, value: 1.0 },
            { key: "guides", type: "bool", label: "Guide lines", value: true },
        ],
    },
    {
        name: "Hanabi",
        file: "Hanabi/Style.qml",
        description: "花火 — Japanese lyric-PV: words on colour blocks, fireworks, a rising night sky (after Folia)",
        tunables: [
            { key: "sparks", label: "Fireworks", min: 0, max: 2, step: 0.1, value: 1.0 },
            { key: "motion", label: "Word motion", min: 0, max: 2, step: 0.1, value: 1.0 },
            { key: "spin", label: "Camera swing", min: 0, max: 2, step: 0.1, value: 1.0 },
            { key: "coverColors", label: "Use cover colours", min: 0, max: 1, step: 0.05, value: 0.35 },
            { key: "sky", label: "Night sky", min: 0, max: 1, step: 0.05, value: 0.8 },
        ],
    },
    {
        name: "Pendolo",
        file: "Pendolo/Style.qml",
        description: "时计 — lyrics are spokes of a ticking clockwork wheel (after Folia)",
        tunables: [
            { key: "snap", label: "Tick snappiness", min: 0.5, max: 3, step: 0.1, value: 2.0 },
            { key: "activeScale", label: "Current line size", min: 1, max: 1.6, step: 0.05, value: 1.25 },
            { key: "decor", label: "Clockwork brightness", min: 0.2, max: 1, step: 0.05, value: 0.6 },
            { key: "coverOnDial", type: "bool", label: "Cover on the dial", value: true },
        ],
    },
    {
        name: "Still",
        file: "Still/Style.qml",
        description: "Calm, readable column (used for unsynced lyrics)",
        tunables: [
            { key: "size", label: "Text size", min: 0.6, max: 1.6, step: 0.05, value: 1.0 },
        ],
    },
]

// Style used automatically for lyrics without timestamps.
var plainStyle = "Still"

function indexOf(name) {
    for (var i = 0; i < styles.length; ++i)
        if (styles[i].name === name)
            return i
    return 0
}

function byName(name) {
    return styles[indexOf(name)]
}

function next(name) {
    return styles[(indexOf(name) + 1) % styles.length].name
}

function names() {
    return styles.map(function (s) { return s.name })
}

// Defaults merged with saved values: {key: value}.
function settingsFor(name, settings) {
    var entry = byName(name)
    var out = {}
    entry.tunables.forEach(function (t) {
        var v = settings.styleValue(entry.name, t.key, t.value)
        // QSettings returns strings for values read back from the file.
        if (t.type === "bool")
            v = (v === true || v === "true")
        else
            v = Number(v)
        out[t.key] = v
    })
    return out
}
