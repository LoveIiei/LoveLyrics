// SPDX-License-Identifier: AGPL-3.0-or-later
// Settings panel (slides in from the right). Every change is saved immediately.
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Material
import LoveLyrics.Backend
import "styles/registry.js" as Registry

Item {
    id: page
    required property var theme
    property bool open: false
    signal closeRequested()

    visible: panel.x < page.width

    // Dim backdrop; click outside the panel to close.
    Rectangle {
        anchors.fill: parent
        color: "black"
        opacity: page.open ? 0.35 : 0
        Behavior on opacity { NumberAnimation { duration: 220 } }
        MouseArea {
            anchors.fill: parent
            enabled: page.open
            onClicked: page.closeRequested()
        }
    }

    Pane {
        id: panel
        Material.theme: Material.Dark
        Material.accent: page.theme.accent
        Material.background: Qt.rgba(0.07, 0.07, 0.09, 0.94)
        width: Math.min(page.width, 520)
        height: page.height
        x: page.open ? page.width - width : page.width
        Behavior on x { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
        padding: 0

        // Swallow clicks so they don't reach the backdrop.
        MouseArea { anchors.fill: parent }

        ScrollView {
            id: scroll
            anchors.fill: parent
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: scroll.availableWidth
                spacing: 6

                component SectionTitle: Label {
                    Layout.topMargin: 18
                    Layout.leftMargin: 24
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    font.capitalization: Font.AllUppercase
                    opacity: 0.6
                }
                component Row2: RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    spacing: 12
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 16
                    Label {
                        text: qsTr("Settings")
                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                    }
                    ToolButton {
                        text: "✕"
                        onClicked: page.closeRequested()
                    }
                }

                // ---- Player -------------------------------------------------------------
                SectionTitle { text: qsTr("Player") }
                Row2 {
                    Label { text: qsTr("MPRIS name"); Layout.preferredWidth: 150 }
                    TextField {
                        Layout.fillWidth: true
                        text: Settings.playerName
                        placeholderText: "strawberry"
                        onEditingFinished: Settings.playerName = text.trim() || "strawberry"
                    }
                }
                Label {
                    Layout.leftMargin: 24
                    opacity: 0.6
                    font.pixelSize: 13
                    text: Player.connected ? qsTr("Connected to %1").arg(Player.identity || Player.serviceName)
                                           : qsTr("Not running")
                }

                // ---- Lyrics --------------------------------------------------------------
                SectionTitle { text: qsTr("Lyrics") }
                Label {
                    Layout.leftMargin: 24
                    text: qsTr("Where to look first")
                    opacity: 0.8
                }
                Repeater {
                    id: sources
                    model: Settings.lyricSourceOrder
                    readonly property var labels: ({ sidecar: qsTr(".lrc file next to the song"),
                                                     embedded: qsTr("Lyrics in the file's tags"),
                                                     mpris: qsTr("Lyrics sent by the player") })
                    delegate: Row2 {
                        required property string modelData
                        required property int index
                        Label {
                            text: (index + 1) + ".  " + (sources.labels[modelData] ?? modelData)
                            Layout.fillWidth: true
                        }
                        ToolButton {
                            text: "▲"
                            enabled: index > 0
                            onClicked: page.moveSource(index, -1)
                        }
                        ToolButton {
                            text: "▼"
                            enabled: index < sources.count - 1
                            onClicked: page.moveSource(index, 1)
                        }
                    }
                }
                Row2 {
                    Label { text: qsTr("Global offset (ms)"); Layout.fillWidth: true }
                    SpinBox {
                        from: -10000
                        to: 10000
                        stepSize: 50
                        editable: true
                        value: Settings.globalOffsetMs
                        onValueModified: Settings.globalOffsetMs = value
                    }
                }
                Label {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.55
                    font.pixelSize: 12
                    text: qsTr("Positive values show lyrics later. Per-track offset: [ and ] (now %1 ms).").arg(Clock.trackOffsetMs)
                }
                Row2 {
                    Label { text: qsTr("Show translations"); Layout.fillWidth: true }
                    Switch {
                        checked: Settings.showTranslation
                        onToggled: Settings.showTranslation = checked
                    }
                }

                Row2 {
                    Label { text: qsTr("Look up lyrics online (LRCLIB)"); Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Switch {
                        checked: Settings.onlineLyrics
                        onToggled: Settings.onlineLyrics = checked
                    }
                }
                Row2 {
                    visible: Settings.onlineLyrics
                    Label { text: qsTr("Also save them as .lrc next to the song"); Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Switch {
                        checked: Settings.saveOnlineLyrics
                        onToggled: Settings.saveOnlineLyrics = checked
                    }
                }
                Label {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.55
                    font.pixelSize: 12
                    text: qsTr("When a song has no synced lyrics, its artist, title, album and length are sent to lrclib.net. "
                               + "Downloads are kept in ~/.cache/lovelyrics/lyrics, so each song is looked up once.")
                }

                // ---- Style ---------------------------------------------------------------
                SectionTitle { text: qsTr("Style") }
                Row2 {
                    Label { text: qsTr("Style"); Layout.preferredWidth: 150 }
                    ComboBox {
                        id: styleBox
                        Layout.fillWidth: true
                        model: Registry.names()
                        currentIndex: Registry.indexOf(Settings.style)
                        onActivated: (i) => Settings.style = Registry.names()[i]
                    }
                }
                Label {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.6
                    font.pixelSize: 13
                    text: Registry.byName(Settings.style).description
                          + (Lyrics.hasLyrics && !Lyrics.synced ? qsTr(" — this song has unsynced lyrics, shown in Still.") : "")
                }
                Repeater {
                    model: Registry.byName(Settings.style).tunables
                    delegate: Row2 {
                        id: tunable
                        required property var modelData
                        readonly property string styleName: Settings.style
                        property var value: Registry.settingsFor(styleName, Settings)[modelData.key]
                        Label { text: tunable.modelData.label; Layout.preferredWidth: 200; wrapMode: Text.Wrap }
                        Slider {
                            visible: tunable.modelData.type !== "bool"
                            Layout.fillWidth: true
                            from: tunable.modelData.min ?? 0
                            to: tunable.modelData.max ?? 1
                            stepSize: tunable.modelData.step ?? 0.1
                            snapMode: Slider.SnapAlways
                            value: tunable.modelData.type !== "bool" ? Number(tunable.value) : 0
                            onMoved: Settings.setStyleValue(tunable.styleName, tunable.modelData.key, value)
                        }
                        Label {
                            visible: tunable.modelData.type !== "bool"
                            Layout.preferredWidth: 44
                            horizontalAlignment: Text.AlignRight
                            text: Number(tunable.value).toFixed((tunable.modelData.step ?? 1) < 1 ? 2 : 0)
                            opacity: 0.7
                        }
                        Item { Layout.fillWidth: true; visible: tunable.modelData.type === "bool" }
                        Switch {
                            visible: tunable.modelData.type === "bool"
                            checked: tunable.value === true
                            onToggled: Settings.setStyleValue(tunable.styleName, tunable.modelData.key, checked)
                        }
                        Connections {
                            target: Settings
                            function onStyleValueChanged() {
                                tunable.value = Registry.settingsFor(tunable.styleName, Settings)[tunable.modelData.key]
                            }
                        }
                    }
                }
                Row2 {
                    Label { text: qsTr("Background"); Layout.preferredWidth: 150 }
                    ComboBox {
                        Layout.fillWidth: true
                        readonly property var values: ["cover", "gradient"]
                        model: [qsTr("Blurred cover"), qsTr("Colour gradient")]
                        currentIndex: Math.max(0, values.indexOf(Settings.backgroundMode))
                        onActivated: (i) => Settings.backgroundMode = values[i]
                    }
                }
                Row2 {
                    Label { text: qsTr("Background pulse on lines"); Layout.preferredWidth: 150; wrapMode: Text.Wrap }
                    Slider {
                        Layout.fillWidth: true
                        from: 0
                        to: 1
                        stepSize: 0.05
                        value: Settings.backgroundPulse
                        onMoved: Settings.backgroundPulse = value
                    }
                }
                Row2 {
                    Label { text: qsTr("Cover and title in the corner"); Layout.fillWidth: true }
                    Switch {
                        checked: Settings.showCoverInfo
                        onToggled: Settings.showCoverInfo = checked
                    }
                }

                // ---- Text ----------------------------------------------------------------
                SectionTitle { text: qsTr("Text") }
                Row2 {
                    Label { text: qsTr("Font"); Layout.preferredWidth: 150 }
                    ComboBox {
                        id: fontBox
                        Layout.fillWidth: true
                        editable: true
                        model: Qt.fontFamilies()
                        Component.onCompleted: {
                            const i = find(Settings.fontFamily)
                            if (i >= 0)
                                currentIndex = i
                            else
                                editText = Settings.fontFamily
                        }
                        onActivated: (i) => Settings.fontFamily = textAt(i)
                        onAccepted: Settings.fontFamily = editText
                    }
                }
                Row2 {
                    Label { text: qsTr("Weight"); Layout.preferredWidth: 150 }
                    ComboBox {
                        Layout.fillWidth: true
                        readonly property var weights: [300, 400, 500, 600, 700, 800, 900]
                        model: [qsTr("Light"), qsTr("Regular"), qsTr("Medium"), qsTr("Semi-bold"), qsTr("Bold"),
                                qsTr("Extra-bold"), qsTr("Black")]
                        currentIndex: Math.max(0, weights.indexOf(Settings.fontWeight))
                        onActivated: (i) => Settings.fontWeight = weights[i]
                    }
                }
                Row2 {
                    Label { text: qsTr("Size"); Layout.preferredWidth: 150 }
                    Slider {
                        Layout.fillWidth: true
                        from: 0.6
                        to: 1.6
                        stepSize: 0.05
                        snapMode: Slider.SnapAlways
                        value: Settings.fontScale
                        onMoved: Settings.fontScale = value
                    }
                    Label { text: Settings.fontScale.toFixed(2) + "×"; opacity: 0.7; Layout.preferredWidth: 44 }
                }

                // ---- Display -------------------------------------------------------------
                SectionTitle { text: qsTr("Display") }
                Row2 {
                    Label { text: qsTr("Full screen on"); Layout.preferredWidth: 150 }
                    ComboBox {
                        Layout.fillWidth: true
                        readonly property var names: [""].concat(Qt.application.screens.map(s => s.name))
                        model: [qsTr("The window's monitor")].concat(Qt.application.screens.map(
                                   s => s.name + "  (" + s.width + "×" + s.height + (s.model ? ", " + s.model : "") + ")"))
                        currentIndex: Math.max(0, names.indexOf(Settings.fullScreenScreen))
                        onActivated: (i) => Settings.fullScreenScreen = names[i]
                    }
                }
                Row2 {
                    Label { text: qsTr("Hide the control bar"); Layout.preferredWidth: 150; wrapMode: Text.Wrap }
                    ComboBox {
                        Layout.fillWidth: true
                        readonly property var modes: ["always", "fullscreen", "never"]
                        model: [qsTr("When idle"), qsTr("When idle, in full screen only"), qsTr("Never (always visible)")]
                        currentIndex: Math.max(0, modes.indexOf(Settings.controlsAutoHide))
                        onActivated: (i) => Settings.controlsAutoHide = modes[i]
                    }
                }
                Row2 {
                    Label { text: qsTr("Frame-rate cap"); Layout.preferredWidth: 150 }
                    ComboBox {
                        Layout.fillWidth: true
                        readonly property var caps: [0, 60, 120]
                        model: [qsTr("Off (display rate)"), "60 fps", "120 fps"]
                        currentIndex: Math.max(0, caps.indexOf(Settings.fpsCap))
                        onActivated: (i) => Settings.fpsCap = caps[i]
                    }
                }

                // ---- Start ---------------------------------------------------------------
                SectionTitle { text: qsTr("Start") }
                Row2 {
                    Label { text: qsTr("Start in the background at login"); Layout.fillWidth: true; wrapMode: Text.Wrap }
                    Switch {
                        checked: Autostart.startAtLogin
                        onToggled: Autostart.startAtLogin = checked
                    }
                }
                Row2 {
                    Label { text: qsTr("When playback starts"); Layout.preferredWidth: 150; wrapMode: Text.Wrap }
                    ComboBox {
                        Layout.fillWidth: true
                        readonly property var modes: ["off", "window", "fullscreen"]
                        model: [qsTr("Do nothing"), qsTr("Show the window"), qsTr("Show full screen")]
                        currentIndex: Math.max(0, modes.indexOf(Settings.autostartMode))
                        onActivated: (i) => Settings.autostartMode = modes[i]
                    }
                }
                Row2 {
                    Label { text: qsTr("Hide when the player quits"); Layout.fillWidth: true }
                    Switch {
                        checked: Settings.hideWhenPlayerQuits
                        onToggled: Settings.hideWhenPlayerQuits = checked
                    }
                }
                Label {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.55
                    font.pixelSize: 12
                    text: (Autostart.resident
                           ? qsTr("Closing the window keeps Love Lyrics listening in the background; it comes back when playback starts. Quit with Ctrl+Q or the button below.")
                           : qsTr("These apply while Love Lyrics runs in the background: turn on start at login, or run lovelyrics --background."))
                          + " " + qsTr("Full screen uses the monitor chosen under Display.")
                }
                Row2 {
                    Item { Layout.fillWidth: true }
                    Button {
                        text: qsTr("Quit Love Lyrics")
                        flat: true
                        onClicked: Qt.quit()
                    }
                }

                // ---- About ---------------------------------------------------------------
                SectionTitle { text: qsTr("About") }
                Label {
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.bottomMargin: 24
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.6
                    font.pixelSize: 12
                    text: Qt.application.displayName + " " + Qt.application.version
                          + qsTr(" — AGPL-3.0-or-later. Ideas from Folia (chthollyphile/folia-major).")
                          + "\n" + qsTr("Settings file: %1").arg(Settings.fileName)
                }
            }
        }
    }

    function moveSource(index, delta) {
        const order = Settings.lyricSourceOrder.slice()
        const j = index + delta
        if (j < 0 || j >= order.length)
            return
        const tmp = order[index]
        order[index] = order[j]
        order[j] = tmp
        Settings.lyricSourceOrder = order
    }
}
