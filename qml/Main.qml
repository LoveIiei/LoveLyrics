// SPDX-License-Identifier: AGPL-3.0-or-later
import QtQuick
import QtQuick.Window
import LoveLyrics.Backend

Window {
    id: root
    width: 1280
    height: 800
    visible: true
    color: "#101014"
    title: Player.title ? Player.title + " — " + Qt.application.displayName : Qt.application.displayName

    property bool startOnDebugPage: false

    // One clock update per rendered frame, so everything moves in step with vsync.
    FrameAnimation {
        running: Clock.playing
        onTriggered: Clock.tick()
    }

    DebugPage {
        anchors.fill: parent
    }

    Shortcut {
        sequence: "Space"
        onActivated: Player.playPause()
    }
    Shortcut {
        sequence: "Right"
        onActivated: Player.next()
    }
    Shortcut {
        sequence: "Left"
        onActivated: Player.previous()
    }
    Shortcut {
        sequence: "["
        onActivated: App.adjustTrackOffset(-100)
    }
    Shortcut {
        sequence: "]"
        onActivated: App.adjustTrackOffset(100)
    }
}
