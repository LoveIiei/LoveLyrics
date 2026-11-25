// SPDX-License-Identifier: AGPL-3.0-or-later
// The palette with cross-fades: every colour eases to the new track's palette over ~800 ms.
import QtQuick
import LoveLyrics.Backend

Item {
    id: theme
    visible: false

    property int fadeMs: 800

    property color dominant: Palette.dominant
    property color vibrant: Palette.vibrant
    property color muted: Palette.muted
    property color dark: Palette.dark
    property color light: Palette.light
    property color accent: Palette.accent
    property color text: Palette.text

    Behavior on dominant { ColorAnimation { duration: theme.fadeMs; easing.type: Easing.InOutQuad } }
    Behavior on vibrant { ColorAnimation { duration: theme.fadeMs; easing.type: Easing.InOutQuad } }
    Behavior on muted { ColorAnimation { duration: theme.fadeMs; easing.type: Easing.InOutQuad } }
    Behavior on dark { ColorAnimation { duration: theme.fadeMs; easing.type: Easing.InOutQuad } }
    Behavior on light { ColorAnimation { duration: theme.fadeMs; easing.type: Easing.InOutQuad } }
    Behavior on accent { ColorAnimation { duration: theme.fadeMs; easing.type: Easing.InOutQuad } }
    Behavior on text { ColorAnimation { duration: theme.fadeMs; easing.type: Easing.InOutQuad } }

    // Font settings shared by all styles.
    readonly property string fontFamily: Settings.fontFamily
    readonly property int fontWeight: Settings.fontWeight
    readonly property real fontScale: Settings.fontScale
}
