// SPDX-License-Identifier: AGPL-3.0-or-later
// Lyric card editor: live preview on the left (click a line on the card to highlight it),
// options on the right, export as PNG or copy to the clipboard.
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Material
import LoveLyrics.Backend

Item {
    id: editor
    required property var theme
    property bool open: false
    signal closeRequested()
    signal message(string text)

    visible: opacity > 0
    opacity: open ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 200 } }
    onOpenChanged: open ? Cards.open() : Cards.close()

    readonly property var st: Cards.state
    readonly property var tinfo: Cards.templateInfo

    Connections {
        target: Cards
        function onExportFinished(ok, pathOrError) {
            if (!ok)
                editor.message(pathOrError)
            else if (pathOrError === "")
                editor.message(qsTr("Card copied to the clipboard"))
            else
                editor.message(qsTr("Saved to %1").arg(pathOrError))
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0.04, 0.04, 0.06, 0.92)
        MouseArea { anchors.fill: parent } // swallow clicks
    }

    // ---- preview -----------------------------------------------------------------------
    Item {
        id: previewArea
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: panel.left
        anchors.margins: 32

        Flickable {
            id: previewFlick
            anchors.fill: parent
            contentWidth: width
            contentHeight: Math.max(height, cardBox.height)
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Item {
                id: cardBox
                // Fit the card to the area (cards taller than 9:16 scroll).
                width: Math.min(previewFlick.width * 0.92, 620,
                                previewFlick.height / Math.min(Math.max(Cards.previewAspect, 0.3), 1.78))
                height: width * Cards.previewAspect
                x: (previewFlick.width - width) / 2
                y: Math.max(0, (previewFlick.height - height) / 2)

                Rectangle { // shadow-ish frame
                    anchors.fill: parent
                    anchors.margins: -1
                    color: "transparent"
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    radius: 2
                }
                Image {
                    id: preview
                    anchors.fill: parent
                    source: Cards.previewSource
                    fillMode: Image.Stretch
                    smooth: true
                    mipmap: true
                    cache: false
                    asynchronous: false
                }
                Repeater {
                    model: Cards.hits
                    delegate: MouseArea {
                        required property var modelData
                        x: modelData.x * cardBox.width
                        y: modelData.y * cardBox.height
                        width: modelData.w * cardBox.width
                        height: modelData.h * cardBox.height
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Cards.toggleFocus(modelData.line)
                        Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            radius: 6
                            border.width: parent.containsMouse ? 1 : 0
                            border.color: Qt.rgba(1, 1, 1, 0.35)
                        }
                    }
                }
            }
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: -24
            Material.theme: Material.Dark
            opacity: 0.6
            font.pixelSize: 12
            text: Cards.dropped > 0
                  ? qsTr("%n line(s) didn't fit this size and were left out", "", Cards.dropped)
                  : qsTr("Click a line on the card to highlight it")
        }
    }

    // ---- options -------------------------------------------------------------------------
    Pane {
        id: panel
        Material.theme: Material.Dark
        Material.accent: editor.theme.accent
        Material.background: Qt.rgba(0.08, 0.08, 0.10, 1)
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.min(460, editor.width * 0.42)
        padding: 0

        ScrollView {
            id: scroll
            anchors.fill: parent
            anchors.bottomMargin: exportBar.height
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: scroll.availableWidth
                spacing: 4

                component SectionTitle: Label {
                    Layout.topMargin: 16
                    Layout.leftMargin: 20
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    font.capitalization: Font.AllUppercase
                    opacity: 0.6
                }
                component Row2: RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    spacing: 10
                }
                // A row of exclusive choice chips.
                component Choices: Flow {
                    id: choices
                    property var options: []      // [{value, label, enabled?}]
                    property var current
                    signal picked(var value)
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    spacing: 6
                    Repeater {
                        model: choices.options
                        delegate: Button {
                            required property var modelData
                            text: modelData.label
                            checkable: true
                            checked: modelData.value === choices.current
                            enabled: modelData.enabled !== false
                            flat: !checked
                            highlighted: checked
                            font.pixelSize: 13
                            onClicked: choices.picked(modelData.value)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 12
                    Label {
                        text: qsTr("Lyric card")
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                    }
                    ToolButton { text: "✕"; onClicked: editor.closeRequested() }
                }

                // ---- lines
                SectionTitle { text: qsTr("Lines") }
                Label {
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.55
                    font.pixelSize: 12
                    text: qsTr("Tick lines to put them on the card; ★ highlights a line.")
                }
                ListView {
                    id: lineList
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    Layout.preferredHeight: Math.min(contentHeight, 260)
                    clip: true
                    model: Cards.lines
                    ScrollBar.vertical: ScrollBar {}
                    delegate: RowLayout {
                        required property var modelData
                        width: lineList.width
                        spacing: 0
                        CheckBox {
                            checked: modelData.selected
                            onClicked: Cards.toggleLine(modelData.index)
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.text
                            elide: Text.ElideRight
                            opacity: modelData.selected ? 1 : 0.55
                            MouseArea { anchors.fill: parent; onClicked: Cards.toggleLine(modelData.index) }
                        }
                        ToolButton {
                            text: modelData.focused ? "★" : "☆"
                            opacity: modelData.focused ? 1 : 0.45
                            onClicked: Cards.toggleFocus(modelData.index)
                        }
                    }
                    // Start with the selected lines in view.
                    onCountChanged: Qt.callLater(() => {
                        for (let i = 0; i < count; ++i)
                            if (model[i].selected) { positionViewAtIndex(i, ListView.Beginning); break }
                    })
                }

                // ---- template
                SectionTitle { text: qsTr("Template") }
                Choices {
                    options: Cards.templates.map(t => ({ value: t.id, label: t.label }))
                    current: editor.st.template
                    onPicked: (v) => Cards.set("template", v)
                }
                Choices {
                    options: [
                        { value: "auto", label: qsTr("Long"), enabled: editor.tinfo.ratios.indexOf("auto") >= 0 },
                        { value: "4:5", label: "4:5" },
                        { value: "9:16", label: "9:16" },
                        { value: "1:1", label: "1:1" },
                    ]
                    current: editor.tinfo.effectiveRatio
                    onPicked: (v) => Cards.set("ratio", v)
                }

                // ---- colour
                SectionTitle { text: qsTr("Colour") }
                Flow {
                    Layout.fillWidth: true
                    Layout.leftMargin: 20
                    Layout.rightMargin: 20
                    spacing: 8
                    Repeater {
                        model: Cards.swatches
                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            width: 32
                            height: 32
                            radius: 16
                            color: modelData.color
                            border.width: editor.st.seedIndex === index ? 3 : 1
                            border.color: editor.st.seedIndex === index ? "white" : Qt.rgba(1, 1, 1, 0.25)
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: Cards.set("seedIndex", editor.st.seedIndex === index ? -1 : index)
                            }
                        }
                    }
                }
                Choices {
                    visible: editor.tinfo.style
                    options: [
                        { value: "content", label: qsTr("Faithful") },
                        { value: "tonal", label: qsTr("Soft") },
                        { value: "vibrant", label: qsTr("Vivid") },
                        { value: "mono", label: qsTr("Mono") },
                    ]
                    current: editor.st.style
                    onPicked: (v) => Cards.set("style", v)
                }
                Choices {
                    options: [
                        { value: "auto", label: qsTr("Auto") },
                        { value: "light", label: qsTr("Light") },
                        { value: "dark", label: qsTr("Dark") },
                    ]
                    current: editor.st.mode
                    onPicked: (v) => Cards.set("mode", v)
                }
                Choices {
                    visible: editor.tinfo.blur
                    options: [
                        { value: "light", label: qsTr("Light blur") },
                        { value: "mid", label: qsTr("Medium blur") },
                        { value: "heavy", label: qsTr("Heavy blur") },
                    ]
                    current: editor.st.blur
                    onPicked: (v) => Cards.set("blur", v)
                }
                Row2 {
                    visible: editor.tinfo.crop
                    Label { text: qsTr("Framing"); Layout.preferredWidth: 90 }
                    Slider {
                        Layout.fillWidth: true
                        from: 0
                        to: 1
                        value: editor.st.depthX < 0 ? 0.5 : editor.st.depthX
                        onMoved: Cards.set("depthX", value)
                    }
                    ToolButton { text: qsTr("Auto"); onClicked: Cards.set("depthX", -1) }
                }

                // ---- text
                SectionTitle { text: qsTr("Text") }
                Row2 {
                    Label { text: qsTr("Title"); Layout.preferredWidth: 60 }
                    TextField {
                        Layout.fillWidth: true
                        text: editor.st.title
                        onTextEdited: Cards.set("title", text)
                    }
                }
                Row2 {
                    Label { text: qsTr("Artist"); Layout.preferredWidth: 60 }
                    TextField {
                        Layout.fillWidth: true
                        text: editor.st.artist
                        onTextEdited: Cards.set("artist", text)
                    }
                }
                Choices {
                    options: [
                        { value: "sans", label: qsTr("Sans") },
                        { value: "serif", label: qsTr("Serif") },
                        { value: "app", label: qsTr("App font") },
                    ]
                    current: editor.st.font
                    onPicked: (v) => Cards.set("font", v)
                }
                Row2 {
                    Label { text: qsTr("Size"); Layout.preferredWidth: 60 }
                    Slider {
                        Layout.fillWidth: true
                        from: 0.7
                        to: 1.4
                        stepSize: 0.05
                        snapMode: Slider.SnapAlways
                        value: editor.st.scale
                        onMoved: Cards.set("scale", value)
                    }
                }
                Choices {
                    options: [{ value: "left", label: qsTr("Left") }, { value: "center", label: qsTr("Centre") }]
                    current: editor.st.align
                    onPicked: (v) => Cards.set("align", v)
                }
                Row2 {
                    Label { text: qsTr("Translations"); Layout.fillWidth: true }
                    Switch {
                        checked: editor.st.showTranslation
                        onToggled: Cards.set("showTranslation", checked)
                    }
                }

                // ---- extras
                SectionTitle { text: qsTr("Extras") }
                Row2 {
                    Label { text: qsTr("Date"); Layout.preferredWidth: 60 }
                    TextField {
                        Layout.fillWidth: true
                        enabled: editor.st.dateOn
                        text: editor.st.dateText
                        onTextEdited: Cards.set("dateText", text)
                    }
                    Switch { checked: editor.st.dateOn; onToggled: Cards.set("dateOn", checked) }
                }
                Row2 {
                    visible: editor.tinfo.progress
                    Label { text: qsTr("Progress bar"); Layout.fillWidth: true }
                    Switch { checked: editor.st.progressOn; onToggled: Cards.set("progressOn", checked) }
                }
                Row2 {
                    visible: editor.tinfo.note
                    Label { text: qsTr("Note"); Layout.preferredWidth: 60 }
                    TextField {
                        Layout.fillWidth: true
                        enabled: editor.st.noteOn
                        text: editor.st.noteText
                        placeholderText: qsTr("A line in the corner")
                        onTextEdited: Cards.set("noteText", text)
                    }
                    Switch { checked: editor.st.noteOn; onToggled: Cards.set("noteOn", checked) }
                }
                Item { Layout.preferredHeight: 16 }
            }
        }

        // ---- export bar
        Rectangle {
            id: exportBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 72
            color: Qt.rgba(0.06, 0.06, 0.08, 1)
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 8
                ComboBox {
                    id: widthBox
                    Layout.preferredWidth: 120
                    readonly property var widths: [1080, 2160]
                    model: ["1080 px", "2160 px"]
                    currentIndex: Math.max(0, widths.indexOf(editor.st.exportWidth))
                    onActivated: (i) => Cards.set("exportWidth", widths[i])
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Copy")
                    flat: true
                    enabled: !Cards.busy
                    onClicked: Cards.copyToClipboard(widthBox.widths[widthBox.currentIndex])
                }
                Button {
                    text: Cards.busy ? qsTr("Saving…") : qsTr("Save PNG")
                    highlighted: true
                    enabled: !Cards.busy
                    onClicked: Cards.exportPng(widthBox.widths[widthBox.currentIndex])
                }
            }
        }
    }
}
