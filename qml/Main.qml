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
    visible: false // shown by C++ via present()
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

    // ---- styles: two slots, cross-faded on switch ----------------------------------------
    // Unsynced lyrics always use the plain style; otherwise the chosen one.
    readonly property string wantedStyle: Lyrics.hasLyrics && !Lyrics.synced ? Registry.plainStyle : Settings.style
    property string shownStyle: ""
    property int styleRevision: 0 // bumps when a tunable changes
    Connections {
        target: Settings
        function onStyleValueChanged() { root.styleRevision++ }
    }
    // While lyrics are loading (hasLyrics briefly false) keep the current style, so a
    // plain-lyrics track doesn't load the synced style first.
    onWantedStyleChanged: if (Lyrics.hasLyrics || shownStyle === "") Qt.callLater(switchStyle)
    Connections {
        target: Lyrics
        function onDocumentChanged() { if (Lyrics.hasLyrics) Qt.callLater(root.switchStyle) }
    }
    function switchStyle() {
        if (wantedStyle === shownStyle)
            return
        const next = slotA.current ? slotB : slotA
        const prev = slotA.current ? slotA : slotB
        next.load(wantedStyle)
        next.current = true
        prev.current = false
        shownStyle = wantedStyle
    }

    component StyleSlot: Loader {
        id: slot
        property bool current: false
        property string styleName: ""
        anchors.fill: parent
        opacity: current ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 700; easing.type: Easing.InOutQuad } }
        // Unload once faded out: an invisible style costs nothing.
        onOpacityChanged: if (!current && opacity === 0) setSource("")

        function load(name) {
            const entry = Registry.byName(name)
            styleName = entry.name
            setSource(Qt.resolvedUrl("styles/" + entry.file), {
                lines: Lyrics,
                cursor: Cursor,
                positionMs: Clock.lyricsPositionMs,
                theme: theme,
                coverSource: Cover.source,
                styleSettings: Registry.settingsFor(entry.name, Settings),
                showTranslation: Settings.showTranslation,
                showCoverInfo: Settings.showCoverInfo,
            })
        }

        Binding { target: slot.item; property: "positionMs"; value: Clock.lyricsPositionMs; when: slot.item !== null }
        Binding { target: slot.item; property: "coverSource"; value: Cover.source; when: slot.item !== null }
        Binding { target: slot.item; property: "showTranslation"; value: Settings.showTranslation; when: slot.item !== null }
        Binding { target: slot.item; property: "showCoverInfo"; value: Settings.showCoverInfo; when: slot.item !== null }
        Binding { target: slot.item; property: "animate"; value: root.renderActive && slot.current; when: slot.item !== null }
        Binding {
            target: slot.item
            property: "styleSettings"
            when: slot.item !== null
            value: { root.styleRevision; return Registry.settingsFor(slot.styleName, Settings) }
        }
    }

    Item {
        id: styleHost
        anchors.fill: parent
        opacity: root.showLyrics ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 500 } }
        StyleSlot { id: slotA }
        StyleSlot { id: slotB }
        Component.onCompleted: root.switchStyle()
    }

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
        shown: root.controlsShown
        fullScreen: root.isFullScreen
        onToggleFullScreen: root.toggleFullScreen()
        onNextStyle: root.nextStyle()
        onOpenSettings: root.settingsOpen = true
        onOffsetChanged: root.showOffsetToast()
    }

    SettingsPage {
        id: settingsPage
        anchors.fill: parent
        theme: theme
        open: root.settingsOpen
        onCloseRequested: root.settingsOpen = false
    }
    property bool settingsOpen: false

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
            for (const s of Qt.application.screens)
                if (s.name === Settings.fullScreenScreen && s !== root.screen)
                    root.screen = s
            showFullScreen()
        }
    }

    // Called from C++ (WindowManager): show the window, optionally full screen on the
    // chosen monitor, raise it and ask for focus.
    function present(fullScreen) {
        if (fullScreen) {
            for (const s of Qt.application.screens)
                if (s.name === Settings.fullScreenScreen && s !== root.screen)
                    root.screen = s
            showFullScreen()
        } else if (!visible) {
            showNormal()
        }
        raise()
        requestActivate()
    }

    function nextStyle() {
        const name = Registry.next(Settings.style)
        Settings.style = name
        toast.show(Lyrics.hasLyrics && !Lyrics.synced
                   ? qsTr("Style: %1 (this song's lyrics aren't synced, so: %2)").arg(name).arg(Registry.plainStyle)
                   : qsTr("Style: %1").arg(name))
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
    }

    // --- hotkeys -------------------------------------------------------------------------
    Shortcut { sequence: "Space"; enabled: !root.settingsOpen; onActivated: Player.playPause() }
    Shortcut { sequence: "Right"; enabled: !root.settingsOpen; onActivated: Player.next() }
    Shortcut { sequence: "Left"; enabled: !root.settingsOpen; onActivated: Player.previous() }
    Shortcut { sequence: "F"; enabled: !root.settingsOpen; onActivated: root.toggleFullScreen() }
    Shortcut { sequence: "F11"; onActivated: root.toggleFullScreen() }
    Shortcut { sequence: "Escape"; enabled: root.settingsOpen; onActivated: root.settingsOpen = false }
    Shortcut { sequence: "Escape"; enabled: root.isFullScreen && !root.settingsOpen; onActivated: root.showNormal() }
    Shortcut { sequence: ","; onActivated: root.settingsOpen = !root.settingsOpen }
    Shortcut { sequence: "S"; enabled: !root.settingsOpen; onActivated: root.nextStyle() }
    Shortcut { sequence: "["; enabled: !root.settingsOpen; onActivated: { App.adjustTrackOffset(-100); root.showOffsetToast() } }
    Shortcut { sequence: "]"; enabled: !root.settingsOpen; onActivated: { App.adjustTrackOffset(100); root.showOffsetToast() } }
    Shortcut { sequence: "\\"; enabled: !root.settingsOpen; onActivated: { App.resetTrackOffset(); root.showOffsetToast() } }
    Shortcut {
        sequence: "T"
        enabled: !root.settingsOpen
        onActivated: {
            Settings.showTranslation = !Settings.showTranslation
            toast.show(Settings.showTranslation ? qsTr("Translation on") : qsTr("Translation off"))
        }
    }
    Shortcut { sequence: "C"; enabled: !root.settingsOpen; onActivated: Settings.showCoverInfo = !Settings.showCoverInfo }
    Shortcut { sequence: "D"; enabled: !root.settingsOpen; onActivated: root.debugVisible = !root.debugVisible }
    Shortcut { sequences: [StandardKey.Quit]; onActivated: { root.saveGeometry(); Qt.quit() } }
}
