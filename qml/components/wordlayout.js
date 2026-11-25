// SPDX-License-Identifier: AGPL-3.0-or-later
// Balanced word wrapping for animated words (QML's Flow can't centre or balance rows).
.pragma library

// widths[i]: width of word i; gaps[i]: space after word i (0 inside CJK runs).
function greedy(widths, gaps, maxWidth) {
    var rows = [[]]
    var x = 0
    for (var i = 0; i < widths.length; ++i) {
        if (x > 0 && x + widths[i] > maxWidth + 0.5) {
            rows.push([])
            x = 0
        }
        rows[rows.length - 1].push(i)
        x += widths[i] + gaps[i]
    }
    return rows
}

function rowWidth(row, widths, gaps) {
    var w = 0
    for (var k = 0; k < row.length; ++k)
        w += widths[row[k]] + (k + 1 < row.length ? gaps[row[k]] : 0)
    return w
}

// Returns {xs, ys, width, height, rows}. Rows have about the same width (the narrowest
// width that still needs the same number of rows as plain greedy wrapping).
function layout(widths, gaps, maxWidth, lineHeight, align) {
    var n = widths.length
    if (n === 0)
        return { xs: [], ys: [], width: 0, height: 0, rows: 0 }
    var widest = 0
    for (var i = 0; i < n; ++i)
        widest = Math.max(widest, widths[i])
    maxWidth = Math.max(maxWidth, widest)

    var target = greedy(widths, gaps, maxWidth).length
    var lo = widest, hi = maxWidth
    for (var it = 0; it < 18 && hi - lo > 1; ++it) {
        var mid = (lo + hi) / 2
        if (greedy(widths, gaps, mid).length > target)
            lo = mid
        else
            hi = mid
    }
    var rows = greedy(widths, gaps, hi)
    var boxWidth = 0
    var rws = rows.map(function (r) { var w = rowWidth(r, widths, gaps); boxWidth = Math.max(boxWidth, w); return w })

    var xs = new Array(n), ys = new Array(n)
    for (var r = 0; r < rows.length; ++r) {
        var x = align === "right" ? boxWidth - rws[r] : align === "center" ? (boxWidth - rws[r]) / 2 : 0
        for (var k = 0; k < rows[r].length; ++k) {
            var idx = rows[r][k]
            xs[idx] = x
            ys[idx] = r * lineHeight
            x += widths[idx] + gaps[idx]
        }
    }
    return { xs: xs, ys: ys, width: boxWidth, height: rows.length * lineHeight, rows: rows.length }
}
