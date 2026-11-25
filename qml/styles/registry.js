// SPDX-License-Identifier: AGPL-3.0-or-later
// The list of styles. Each entry: name (saved in settings), file (relative to qml/styles),
// default tunables, and whether it can show unsynced (plain) lyrics.
.pragma library

var styles = [
    { name: "Flow", file: "Flow/Style.qml", plain: true, defaults: { blur: 1.0, stagger: 45, anchor: 0.36 } },
]

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
