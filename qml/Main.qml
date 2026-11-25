// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Window
import LoveLyrics.Backend
import "components"
import "background"
import "styles/registry.js" as Registry

Window {
    id: root
    width: 1280
    height: 800
    minimumWidth: 480
    minimumHeight: 320
    visible: true
    color: "black"
    title: Player.title ? Player.title + " — " + Qt.application.displayName : Qt.application.displayName

    property bool startOnDebugPage: false
    property bool debugVisible: startOnDebugPage

    readonly property bool isFullScreen: visibility === Window.FullScreen
    // Nothing to draw when minimised / hidden: stop per-frame work.
    readonly property bool renderActive: visibility !== Window.Minimized && visibility !== Window.Hidden

    readonly property bool idle: !Player.connected || (Player.status === MprisClient.Stopped && Player.title === "")
    readonly property bool showLyrics: !idle && Lyrics.hasLyrics
    readonly property bool showNoLyrics: !idle && !Lyrics.hasLyrics && !Lyrics.loading

    // One clock update per rendered frame, so motion follows vsync.
    FrameAnimation {
        running: Clock.playing && root.renderActive
        onTriggered: Clock.tick()
    }

    Theme { id: theme }

    Ambient {
        id: background
        anchors.fill: parent
        theme: theme
        animate: root.renderActive && Clock.playing

        TapHandler {
            onDoubleTapped: root.toggleFullScreen()
        }
    }

    IdleScreen {
        anchors.fill: parent
        theme: theme
        opacity: root.idle ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 600 } }
        message: !Player.connected ? qsTr("Waiting for %1").arg(root.playerLabel()) : qsTr("Nothing playing")
    }

    NoLyricsScreen {
        anchors.fill: parent
        theme: theme
        opacity: root.showNoLyrics ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 600 } }
    }

    Loader {
        id: styleLoader
        anchors.fill: parent
        opacity: root.showLyrics ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 500 } }

        function load(name) {
            const entry = Registry.byName(name)
            const settings = Object.assign({}, entry.defaults)
            for (const key in entry.defaults)
                settings[key] = Settings.styleValue(entry.name, key, entry.defaults[key])
            setSource(Qt.resolvedUrl("styles/" + entry.file), {
                lines: Lyrics,
                cursor: Cursor,
                positionMs: Clock.lyricsPositionMs,
                theme: theme,
                coverSource: Cover.source,
                styleSettings: settings,
            })
        }
        Component.onCompleted: load(Settings.style)
    }
    Binding { target: styleLoader.item; property: "positionMs"; value: Clock.lyricsPositionMs; when: styleLoader.item !== null }
    Binding { target: styleLoader.item; property: "coverSource"; value: Cover.source; when: styleLoader.item !== null }
    Binding { target: styleLoader.item; property: "showTranslation"; value: Settings.showTranslation; when: styleLoader.item !== null }
    Binding { target: styleLoader.item; property: "showCoverInfo"; value: Settings.showCoverInfo; when: styleLoader.item !== null }
    Binding { target: styleLoader.item; property: "animate"; value: root.renderActive; when: styleLoader.item !== null }

    DebugPage {
        anchors.fill: parent
        visible: root.debugVisible
    }

    // Chrome (controls, toasts) grows with the window: 1x at 900 px tall, up to 2.4x.
    readonly property real uiScale: Math.max(1, Math.min(2.4, height / 900))

    ControlsOverlay {
        id: controls
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: parent.width / root.uiScale
        scale: root.uiScale
        transformOrigin: Item.BottomLeft
        theme: theme
        shown: root.controlsShown && Player.connected
        fullScreen: root.isFullScreen
        onToggleFullScreen: root.toggleFullScreen()
        onNextStyle: root.nextStyle()
        onOffsetChanged: root.showOffsetToast()
    }

    Toast {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 28 * root.uiScale
        scale: root.uiScale
        transformOrigin: Item.Top
    }

    // --- auto-hide of cursor and controls ---------------------------------------------
    property bool controlsShown: true
    Timer {
        id: hideTimer
        interval: 2000
        running: true
        onTriggered: {
            if (controls.hovered)
                restart()
            else
                root.controlsShown = false
        }
    }
    function poke() {
        controlsShown = true
        hideTimer.restart()
    }
    HoverHandler {
        id: windowHover
        cursorShape: root.controlsShown ? Qt.ArrowCursor : Qt.BlankCursor
        onPointChanged: root.poke()
    }

    // --- actions -------------------------------------------------------------------------
    function playerLabel() {
        const n = Player.playerName
        return n.charAt(0).toUpperCase() + n.slice(1)
    }

    function toggleFullScreen() {
        if (isFullScreen)
            showNormal()
        else {
            saveGeometry()
            showFullScreen()
        }
    }

    function nextStyle() {
        const name = Registry.next(Settings.style)
        Settings.style = name
        styleLoader.load(name)
        toast.show(qsTr("Style: %1").arg(name))
    }

    function showOffsetToast() {
        const ms = Clock.trackOffsetMs
        toast.show(ms === 0 ? qsTr("Lyrics offset: none")
                            : qsTr("Lyrics offset: %1%2 s").arg(ms > 0 ? "+" : "").arg((ms / 1000).toFixed(1)))
    }

    function saveGeometry() {
        if (visibility === Window.Windowed)
            Settings.windowGeometry = Qt.rect(x, y, width, height)
        if (screen)
            Settings.windowScreen = screen.name
    }

    onVisibilityChanged: {
        if (root.visibility === Window.FullScreen || root.visibility === Window.Windowed || root.visibility === Window.Maximized)
            Settings.fullScreen = root.visibility === Window.FullScreen
    }
    onClosing: saveGeometry()

    Component.onCompleted: {
        const g = Settings.windowGeometry
        if (g.width > 0 && g.height > 0) {
            width = g.width
            height = g.height
            x = g.x // ignored on Wayland, used on X11
            y = g.y
        }
        for (const s of Qt.application.screens)
            if (s.name === Settings.windowScreen)
                root.screen = s
        if (Settings.fullScreen)
            showFullScreen()
    }

    // --- hotkeys -------------------------------------------------------------------------
    Shortcut { sequence: "Space"; onActivated: Player.playPause() }
    Shortcut { sequence: "Right"; onActivated: Player.next() }
    Shortcut { sequence: "Left"; onActivated: Player.previous() }
    Shortcut { sequence: "F"; onActivated: root.toggleFullScreen() }
    Shortcut { sequence: "F11"; onActivated: root.toggleFullScreen() }
    Shortcut { sequence: "Escape"; enabled: root.isFullScreen; onActivated: root.showNormal() }
    Shortcut { sequence: "S"; onActivated: root.nextStyle() }
    Shortcut { sequence: "["; onActivated: { App.adjustTrackOffset(-100); root.showOffsetToast() } }
    Shortcut { sequence: "]"; onActivated: { App.adjustTrackOffset(100); root.showOffsetToast() } }
    Shortcut { sequence: "\\"; onActivated: { App.resetTrackOffset(); root.showOffsetToast() } }
    Shortcut {
        sequence: "T"
        onActivated: {
            Settings.showTranslation = !Settings.showTranslation
            toast.show(Settings.showTranslation ? qsTr("Translation on") : qsTr("Translation off"))
        }
    }
    Shortcut { sequence: "C"; onActivated: Settings.showCoverInfo = !Settings.showCoverInfo }
    Shortcut { sequence: "D"; onActivated: root.debugVisible = !root.debugVisible }
    Shortcut { sequences: [StandardKey.Quit]; onActivated: { root.saveGeometry(); Qt.quit() } }
}
