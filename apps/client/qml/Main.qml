pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Shapes
import Mnaka.App

ApplicationWindow {
    id: window
    width: 1440; height: 900
    readonly property bool fillsScreen: Qt.platform.os === "wasm" || Qt.platform.os === "android"
    minimumWidth: fillsScreen ? 0 : 380; minimumHeight: fillsScreen ? 0 : 600
    visible: true
    title: "MNAKA · AI workspace"
    font.family: window.mono
    color: "#030806"
    property color ink: "#d2ffe3"
    property color muted: "#5fa37f"
    property color accent: "#00e676"
    property color cyan: "#00ff9c"
    property color panel: "#06110b"
    property color line: "#14402a"
    // Code font: Qt for WebAssembly bundles DejaVu Sans Mono.
    readonly property string mono: Qt.platform.os === "windows" ? "Consolas" : Qt.platform.os === "android" ? "monospace" : "DejaVu Sans Mono"
    property int page: 0
    property bool compact: width < 850
    property bool wide: width >= 1320
    property var pages: ["Overview", "Tasks", "Developer", "Computer Control", "Skills", "Projects", "Vision", "Proactive", "Settings"]
    property var titles: ["Command Centre", "Tasks", "Developer Workspace", "Computer Control", "Skills", "Projects", "Vision", "Proactive", "Settings"]
    property var descriptions: ["Your AI workspace for development, automation and more.", "Review progress, evidence and cancellations.", "From an authorized project to a reviewable change.", "Defined actions. Verified destinations.", "Declarative capabilities, with bounded permissions.", "Sourced facts and decisions, under your control.", "Interpret selected images and explore spatial scenes.", "User-enabled rules with explicit scope.", "Privacy, providers and device preferences."]
    // 24-unit stroke icons, drawn by the Icon component.
    readonly property var icons: ({
        home: "M3 10l9-7 9 7v10a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z M9 22v-9h6v9",
        tasks: "M9 6h12 M9 12h12 M9 18h12 M3.5 6h1 M3.5 12h1 M3.5 18h1",
        code: "M16 18l6-6-6-6 M8 6l-6 6 6 6",
        monitor: "M4 3h16a2 2 0 0 1 2 2v10a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z M8 21h8 M12 17v4",
        layers: "M12 2L2 7l10 5 10-5z M2 17l10 5 10-5 M2 12l10 5 10-5",
        folder: "M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z",
        eye: "M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z M12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6z",
        zap: "M13 2L3 14h9l-1 8 10-12h-9z",
        settings: "M4 21v-7 M4 10V3 M12 21v-9 M12 8V3 M20 21v-5 M20 12V3 M1 14h6 M9 8h6 M17 16h6",
        chart: "M18 20V10 M12 20V4 M6 20v-6",
        send: "M22 2L11 13 M22 2l-7 20-4-9-9-4z",
        bell: "M18 8A6 6 0 0 0 6 8c0 7-3 9-3 9h18s-3-2-3-9 M13.7 21a2 2 0 0 1-3.4 0",
        calendar: "M5 4h14a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2z M16 2v4 M8 2v4 M3 10h18",
        chevron: "M9 18l6-6-6-6",
        check: "M20 6L9 17l-5-5",
        mic: "M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3z M19 10v2a7 7 0 0 1-14 0v-2 M12 19v3 M8 22h8",
        file: "M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z M14 2v6h6",
        down: "M6 9l6 6 6-6",
        ring: "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18z",
        info: "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18z M12 16v-5 M12 7.5v.5"
    })
    readonly property var pageIcons: [icons.home, icons.tasks, icons.code, icons.monitor, icons.layers, icons.folder, icons.eye, icons.zap, icons.settings]
    readonly property bool signedIn: workspace.accountEmail.length > 0
    readonly property string accountName: signedIn ? workspace.accountEmail.split("@")[0] : ""
    readonly property string greeting: { const hour = new Date().getHours(); return hour < 12 ? "Good morning" : hour < 18 ? "Good afternoon" : "Good evening" }
    property int devTab: 0
    // A finished utterance: a few spoken commands act locally, anything else is sent as a message.
    function handleVoice(spoken) {
        const said = spoken.toLowerCase().replace(/[.,!?]/g, "").trim()
        const screens = { "overview": 0, "home": 0, "command centre": 0, "command center": 0, "tasks": 1, "developer": 2, "developer workspace": 2,
            "computer control": 3, "control": 3, "skills": 4, "projects": 5, "memory": 5, "vision": 6, "proactive": 7, "rules": 7, "settings": 8 }
        const go = said.match(/^(?:open|go to|show|switch to)\s+(?:the\s+)?(.+)$/)
        if (go && screens[go[1]] !== undefined) { window.page = screens[go[1]]; window.voiceNotice = "Opened " + window.pages[window.page] + "."; return }
        if (said === "cancel" || said === "stop" || said === "cancel task") { workspace.cancel(); window.voiceNotice = "Cancel requested."; return }
        window.voiceNotice = ""
        window.page = 0
        if (!workspace.submit(spoken, mode.currentText)) composer.text = spoken // keep what was said so it can be sent later
    }
    property string voiceNotice: ""
    readonly property bool voiceActive: workspace.voice.awaitingRequest || (!workspace.voice.handsFree && workspace.voice.listening)
    readonly property bool voiceWaiting: workspace.voice.handsFree && workspace.voice.listening && !workspace.voice.awaitingRequest
    function scrollLimit() { return Math.max(0, scroll.contentHeight - scroll.height) }
    function scrollBy(distance) { window.glideTo((glide.running ? glide.to : scroll.contentItem.contentY) + distance) }
    function glideTo(position) {
        glide.stop()
        glide.to = Math.max(0, Math.min(position, window.scrollLimit()))
        glide.start()
    }
    function wheelScroll(event) {
        const view = scroll.contentItem
        const delta = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y / 120 * 96
        if (Math.abs(delta) < 40 && !glide.running) view.contentY = Math.max(0, Math.min(view.contentY - delta, window.scrollLimit()))
        else window.glideTo((glide.running ? glide.to : view.contentY) - delta)
    }
    NumberAnimation { id: glide; target: scroll.contentItem; property: "contentY"; duration: 220; easing.type: Easing.OutCubic }
    Connections {
        target: scroll.contentItem
        // A finger on the page takes over from a glide in progress.
        function onDraggingChanged() { if (scroll.contentItem.dragging) glide.stop() }
    }
    function revealComposer() {
        const bottom = composerBox.mapToItem(content, 0, composerBox.height).y + 56
        const view = scroll.contentItem
        if (bottom > view.contentY + scroll.height) view.contentY = Math.min(bottom - scroll.height, Math.max(0, scroll.contentHeight - scroll.height))
    }
    // What each part of MNAKA can do right now, in plain words. One source for the Settings list
    // and the Overview cards. States: AVAILABLE works as described; EXPERIMENTAL works but is new
    // or stores without acting; NOT CONFIGURED needs the owner's model; DESKTOP REQUIRED needs a
    // paired desktop app that does not exist yet; SIGN IN needs an account session.
    function serviceRows() {
        const signedIn = workspace.connected
        const noModel = window.modelDown
        const state = (label, tone, text) => ({ label: label, tone: tone, text: text })
        const signIn = text => state("SIGN IN", "off", text)
        const needsModel = workspace.modelStatus === "failing" ? state("NOT WORKING", "warn", "A model is set, but its provider is refusing requests. " + workspace.modelProblem)
            : state("NOT CONFIGURED", "warn", "No model is set up on your account gateway yet.")
        const available = text => state("AVAILABLE", "on", text)
        const experimental = text => state("EXPERIMENTAL", "new", text)
        const row = (key, name, value) => ({ key: key, name: name, label: value.label, tone: value.tone, text: value.text })
        return [
            row("assistant", "Assistant", !signedIn ? signIn("Sign in to talk to the assistant.") : noModel ? needsModel : available("Answers through " + window.modelLabel + ".")),
            row("memory", "Memory", !signedIn ? signIn("Sign in to save facts for the model.") : available("Facts you save are sent to the model with every message.")),
            row("vision", "Vision", !signedIn ? signIn("Image preview works now; sign in to interpret images.") : noModel ? needsModel : experimental("Interprets an image you choose. Needs a model that accepts images.")),
            row("developer", "Developer workspace", !signedIn ? signIn("You can add files now; sign in to ask for changes.") : noModel ? needsModel : experimental("Proposes changes to files you add. It cannot build or test.")),
            row("skills", "Skills", !signedIn ? signIn("Inspection works now; sign in to save manifests.") : experimental("Saves manifests that pass inspection. Nothing runs them yet.")),
            row("proactive", "Proactive rules", !signedIn ? signIn("Sign in to save rules.") : experimental("Saves your rules. Nothing schedules or runs them yet.")),
            row("voice", "Voice input", workspace.voice.available ? experimental(workspace.voice.handsFree ? "Hands-free: say \"Mnaka\" and your request. Recognised by this browser." : "Press the microphone, or turn on hands-free and say \"Mnaka\".") : state("UNAVAILABLE", "off", "Needs the web version in Chrome, Edge or Safari.")),
            row("control", "Computer control", state("DESKTOP REQUIRED", "off", "Needs a paired desktop app, which is not built yet."))
        ]
    }
    readonly property bool modelDown: workspace.modelStatus === "missing" || workspace.modelStatus === "failing"
    readonly property string modelLabel: workspace.modelName.length > 0 ? workspace.modelName : "the configured model"
    function serviceFor(key) {
        const rows = window.serviceRows()
        for (let i = 0; i < rows.length; ++i) if (rows[i].key === key) return rows[i]
        return { key: key, name: key, label: "UNAVAILABLE", tone: "off", text: "" }
    }
    function toneColor(tone) { return tone === "on" ? "#22c55e" : tone === "warn" ? "#f5b544" : tone === "new" ? "#00ff9c" : "#2f5a44" }
    function capabilityFor(id) {
        const list = workspace.capabilities
        for (let i = 0; i < list.length; ++i) if (list[i].id === id) return list[i]
        return { enabled: false, available: false, status: "Service unavailable" }
    }
    function activeTaskCount() {
        let count = 0
        const list = workspace.tasks
        for (let i = 0; i < list.length; ++i) if (list[i].active) ++count
        return count
    }
    WorkspaceModel { id: workspace }
    FileDialog {
        id: projectPicker
        title: "Add project files"
        fileMode: FileDialog.OpenFiles
        onAccepted: workspace.developer.addFiles(selectedFiles)
    }
    FileDialog {
        id: projectSaver
        property string path
        title: "Save " + path
        fileMode: FileDialog.SaveFile
        onAccepted: workspace.developer.saveFile(path, selectedFile)
    }
    FileDialog {
        id: imagePicker
        title: "Select an image"
        nameFilters: ["Images (*.png *.jpg *.jpeg)"]
        onAccepted: workspace.selectImage(selectedFile)
    }
    palette.window: "#030806"
    palette.base: "#071a10"
    palette.text: window.ink
    palette.windowText: window.ink
    palette.button: "#0b2416"
    palette.buttonText: window.ink
    palette.highlight: window.accent
    palette.highlightedText: "#021006"
    palette.placeholderText: window.muted

    component Caption: Label {
        color: window.muted; font.pixelSize: 13; wrapMode: Text.WordWrap
        textFormat: Text.PlainText
    }
    component Heading: Label {
        color: window.ink; font.pixelSize: 19; font.weight: Font.DemiBold
        wrapMode: Text.WordWrap; textFormat: Text.PlainText
    }
    component Card: Rectangle {
        radius: 2; color: window.panel; border.color: window.line
    }
    component Pill: Rectangle {
        id: pill
        property string text
        property color tint: window.cyan
        implicitWidth: pillLabel.implicitWidth + 22; implicitHeight: 26
        radius: 2; color: Qt.rgba(tint.r, tint.g, tint.b, 0.14)
        Label { id: pillLabel; anchors.centerIn: parent; text: pill.text; color: pill.tint; font.pixelSize: 11 }
    }
    component Icon: Shape {
        id: icon
        property string path
        property color tint: window.muted
        property real size: 20
        property real weight: 1.7
        implicitWidth: size; implicitHeight: size
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: icon.tint; strokeWidth: icon.weight; fillColor: "transparent"
            capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
            scale: Qt.size(icon.size / 24, icon.size / 24)
            PathSvg { path: icon.path }
        }
    }
    component Avatar: Rectangle {
        id: avatar
        property string letter
        property bool logo: false
        implicitWidth: 36; implicitHeight: 36; radius: 2
        color: "#0a2014"; border.color: "#1f5a3c"
        Label { visible: !avatar.logo; anchors.centerIn: parent; text: avatar.letter; color: window.ink; font.pixelSize: 14; font.weight: Font.DemiBold }
        Image { visible: avatar.logo; anchors.fill: parent; anchors.margins: 6; source: "assets/logo.png"; sourceSize: Qt.size(64, 64); fillMode: Image.PreserveAspectFit }
    }
    component PrimaryButton: Button {
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        id: primary
        implicitHeight: 38; leftPadding: 18; rightPadding: 18
        contentItem: Label { text: primary.text; color: primary.enabled ? "#021006" : window.muted; font.weight: Font.DemiBold; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 2; color: !primary.enabled ? "#123322" : primary.down ? "#00b85c" : primary.hovered ? "#5dffa6" : window.accent; border.color: primary.activeFocus ? window.cyan : "transparent" }
    }
    component GhostButton: Button {
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        id: ghost
        implicitHeight: 38; leftPadding: 16; rightPadding: 16
        contentItem: Label { text: ghost.text; color: ghost.enabled ? window.ink : window.muted; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 2; color: ghost.hovered ? "#0b2015" : "transparent"; border.color: ghost.activeFocus ? window.cyan : "#1f5a3c" }
    }
    component TermCombo: ComboBox {
        id: combo
        property var notes: [] // one line under each entry saying what choosing it does
        implicitHeight: 40; leftPadding: 12; rightPadding: 34; font.pixelSize: 14
        contentItem: Label { text: combo.displayText; color: window.ink; font.pixelSize: 14; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
        indicator: Label { x: combo.width - width - 12; y: (combo.height - height) / 2; text: combo.popup.visible ? "\u25B4" : "\u25BE"; color: window.cyan; font.pixelSize: 14 }
        background: Rectangle { radius: 2; color: combo.popup.visible || combo.hovered ? "#0b2015" : "#020a06"; border.color: combo.activeFocus || combo.popup.visible ? window.accent : "#1f5a3c" }
        delegate: ItemDelegate {
            id: entry
            required property int index
            required property var modelData
            width: ListView.view.width; padding: 10
            highlighted: combo.highlightedIndex === index
            contentItem: ColumnLayout {
                spacing: 2
                Label { text: (combo.currentIndex === entry.index ? "> " : "  ") + entry.modelData; color: combo.currentIndex === entry.index ? window.cyan : window.ink; font.pixelSize: 14; elide: Text.ElideRight; Layout.fillWidth: true }
                Caption { visible: text.length > 0; text: combo.notes[entry.index] || ""; font.pixelSize: 11; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.leftMargin: 17 }
            }
            background: Rectangle { color: entry.highlighted ? "#0f2b1c" : "transparent" }
        }
        popup: Popup {
            y: combo.height + 2; width: Math.max(combo.width, combo.notes.length > 0 ? 300 : 0); x: combo.width - width; padding: 1
            implicitHeight: Math.min(contentItem.implicitHeight + 2, 420)
            contentItem: ListView { clip: true; implicitHeight: contentHeight; model: combo.popup.visible ? combo.delegateModel : null; currentIndex: combo.highlightedIndex }
            background: Rectangle { color: "#04100a"; border.color: window.accent; radius: 2 }
        }
    }
    component Field: TextField {
        id: field
        color: window.ink; placeholderTextColor: window.muted; font.pixelSize: 14
        leftPadding: 12; rightPadding: 12; implicitHeight: 40
        background: Rectangle { radius: 2; color: "#020a06"; border.color: field.activeFocus ? window.accent : "#1f5a3c" }
    }
    component SectionTitle: RowLayout {
        id: section
        property string icon
        property string text
        property int count: -1
        spacing: 10
        Icon { visible: section.icon.length > 0; path: section.icon.length > 0 ? section.icon : window.icons.check; tint: window.cyan }
        Label { text: "[ " + section.text.toUpperCase() + " ]"; color: window.cyan; font.pixelSize: 13; font.weight: Font.DemiBold; font.letterSpacing: 1 }
        Rectangle {
            visible: section.count >= 0; implicitWidth: 24; implicitHeight: 22; radius: 2; color: "#0a2014"
            Label { anchors.centerIn: parent; text: String(section.count); color: window.muted; font.pixelSize: 11 }
        }
        Item { Layout.fillWidth: true }
    }
    // Decorative state indicator. It turns only while a task is active.
    component Orb: Item {
        id: orb
        property bool active: false
        property real size: 220
        implicitWidth: size; implicitHeight: size
        Shape {
            anchors.fill: parent; preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: "transparent"
                fillGradient: RadialGradient {
                    centerX: orb.size / 2; centerY: orb.size / 2; focalX: centerX; focalY: centerY; centerRadius: orb.size / 2
                    GradientStop { position: 0.0; color: "#6600e676" }
                    GradientStop { position: 0.62; color: "#2a00a050" }
                    GradientStop { position: 1.0; color: "#0000a050" }
                }
                PathAngleArc { centerX: orb.size / 2; centerY: orb.size / 2; radiusX: orb.size / 2; radiusY: orb.size / 2; startAngle: 0; sweepAngle: 360 }
            }
            ShapePath {
                strokeColor: "#5500ff9c"; strokeWidth: 1
                fillGradient: RadialGradient {
                    centerX: orb.size * 0.5; centerY: orb.size * 0.5; focalX: orb.size * 0.38; focalY: orb.size * 0.34; centerRadius: orb.size * 0.36
                    GradientStop { position: 0.0; color: "#e6fff0" }
                    GradientStop { position: 0.3; color: "#19e68c" }
                    GradientStop { position: 0.68; color: "#0a7a48" }
                    GradientStop { position: 1.0; color: "#02170c" }
                }
                PathAngleArc { centerX: orb.size / 2; centerY: orb.size / 2; radiusX: orb.size * 0.34; radiusY: orb.size * 0.34; startAngle: 0; sweepAngle: 360 }
            }
        }
        Item {
            anchors.centerIn: parent; width: orb.size * 0.68; height: width
            RotationAnimator on rotation { running: orb.active; loops: Animation.Infinite; from: 0; to: 360; duration: 7000 }
            Repeater {
                model: [0, 60, 120]
                Shape {
                    id: swirl
                    required property int modelData
                    anchors.fill: parent; rotation: modelData; preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeColor: swirl.modelData === 60 ? "#6600ff9c" : "#55ffffff"; strokeWidth: 1.4; fillColor: "transparent"
                        PathAngleArc { centerX: swirl.width / 2; centerY: swirl.height / 2; radiusX: swirl.width / 2 - 1; radiusY: swirl.width * 0.2; startAngle: 0; sweepAngle: 360 }
                    }
                }
            }
        }
    }
    component DiffLine: Rectangle {
        id: diffLine
        required property var modelData
        readonly property string sign: modelData[1]
        Layout.fillWidth: true; implicitHeight: 22; clip: true
        color: sign === "+" ? "#123222" : sign === "-" ? "#3a1722" : "transparent"
        RowLayout {
            anchors.fill: parent; spacing: 0
            Label { text: String(diffLine.modelData[0]); color: window.muted; font.family: window.mono; font.pixelSize: 12; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 40 }
            Label { text: diffLine.sign; color: diffLine.sign === "+" ? "#4ade80" : "#f87171"; font.family: window.mono; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; Layout.preferredWidth: 30 }
            Label { text: diffLine.modelData[2]; textFormat: Text.PlainText; color: diffLine.sign === "+" ? "#bbf7d0" : diffLine.sign === "-" ? "#fecaca" : window.ink; font.family: window.mono; font.pixelSize: 13; Layout.fillWidth: true }
        }
    }
    component EngineCard: Card {
        id: engine
        property string icon
        property string name
                property string engineId
        property int destination
        readonly property var service: window.serviceFor(engineId)
        implicitHeight: engineColumn.implicitHeight + 28
        color: engineButton.hovered ? "#0b2015" : window.panel
        ColumnLayout {
            id: engineColumn
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 14; spacing: 6
            RowLayout {
                Layout.fillWidth: true
                Icon { path: engine.icon; tint: window.cyan; size: 22 }
                Item { Layout.fillWidth: true }
                Icon { path: window.icons.chevron; size: 16 }
            }
            Label { text: engine.name; color: window.ink; font.pixelSize: 14; font.weight: Font.DemiBold; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.topMargin: 4 }
            Caption { text: engine.service.text; font.pixelSize: 12; Layout.fillWidth: true }
            RowLayout {
                spacing: 7; Layout.topMargin: 2; Layout.fillWidth: true
                Rectangle { implicitWidth: 8; implicitHeight: 8; radius: 4; color: window.toneColor(engine.service.tone) }
                Caption { text: engine.service.label; font.pixelSize: 11; font.letterSpacing: 1; color: engine.service.tone === "off" ? window.muted : window.toneColor(engine.service.tone); elide: Text.ElideRight; wrapMode: Text.NoWrap; Layout.fillWidth: true }
            }
        }
        Button {
            Keys.onReturnPressed: clicked()
            Keys.onEnterPressed: clicked()
            id: engineButton; anchors.fill: parent; flat: true; text: ""
            Accessible.name: "Open " + engine.name
            background: Rectangle { color: "transparent"; radius: 2; border.color: engineButton.activeFocus ? window.cyan : "transparent" }
            onClicked: window.page = engine.destination
        }
    }
    Component.onCompleted: if (workspace.accountNotice.length > 0) window.page = 8
    Shortcut { sequence: "Ctrl+L"; onActivated: { window.page = 0; composer.forceActiveFocus() } }
    Shortcut { sequence: "Escape"; onActivated: workspace.cancel() }
    Shortcut { sequence: "PgDown"; onActivated: window.scrollBy(scroll.height * 0.85) }
    Shortcut { sequence: "PgUp"; onActivated: window.scrollBy(-scroll.height * 0.85) }
    Shortcut { sequence: "Ctrl+End"; onActivated: window.scrollBy(1e9) }
    Shortcut { sequence: "Ctrl+Home"; onActivated: window.scrollBy(-1e9) }
    onPageChanged: { glide.stop(); scroll.contentItem.contentY = 0 }
    Connections {
        target: workspace.voice
        function onRecognized(text) { window.handleVoice(text) }
    }
    Connections {
        target: workspace
        // Keep the composer in view as the conversation grows.
        function onMessagesChanged() { if (window.page === 0) Qt.callLater(window.revealComposer) }
    }

    Canvas {
        anchors.fill: parent; opacity: 0.5
        onPaint: {
            const context = getContext("2d")
            context.clearRect(0, 0, width, height)
            context.fillStyle = "#0a00ff9c"
            for (let y = 0; y < height; y += 4) context.fillRect(0, y, width, 1)
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }
    RowLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            Layout.preferredWidth: 224; Layout.fillHeight: true
            visible: !window.compact; color: "#040b07"
            Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: window.line }
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 14; spacing: 4
                RowLayout {
                    spacing: 10; Layout.topMargin: 8; Layout.leftMargin: 8; Layout.bottomMargin: 22
                    Image { source: "assets/logo.png"; sourceSize: Qt.size(72, 72); Layout.preferredWidth: 34; Layout.preferredHeight: 34; fillMode: Image.PreserveAspectFit; Accessible.name: "MNAKA logo" }
                    Label { text: "MNAKA"; color: window.cyan; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 4 }
                }
                Repeater {
                    model: window.pages
                    Button {
                        Keys.onReturnPressed: clicked()
                        Keys.onEnterPressed: clicked()
                        id: navButton
                        required property int index
                        required property string modelData
                        Layout.fillWidth: true; implicitHeight: 46
                        text: modelData; checkable: true; checked: window.page === index
                        Accessible.name: modelData
                        onClicked: window.page = index
                        contentItem: RowLayout {
                            spacing: 14
                            Icon { path: window.pageIcons[navButton.index]; tint: navButton.checked ? window.cyan : window.muted; Layout.leftMargin: 12 }
                            Label { text: (navButton.checked ? "> " : "  ") + navButton.text.toLowerCase().replace(/ /g, "_"); color: navButton.checked ? window.cyan : window.muted; font.pixelSize: 13; font.weight: navButton.checked ? Font.DemiBold : Font.Normal; Layout.fillWidth: true }
                        }
                        background: Rectangle {
                            radius: 2; border.color: navButton.activeFocus ? window.cyan : "transparent"
                            color: navButton.checked ? "transparent" : navButton.hovered ? "#0b2015" : "transparent"
                            gradient: navButton.checked ? activeNav : null
                            Gradient { id: activeNav; orientation: Gradient.Horizontal; GradientStop { position: 0; color: "#0e3a24" } GradientStop { position: 1; color: "#071a10" } }
                        }
                    }
                }
                Item { Layout.fillHeight: true }
                Button {
                    Keys.onReturnPressed: clicked()
                    Keys.onEnterPressed: clicked()
                    id: accountChip
                    Layout.fillWidth: true; implicitHeight: 54
                    Accessible.name: "Account and settings"
                    onClicked: window.page = 8
                    contentItem: RowLayout {
                        spacing: 10
                        Avatar { letter: window.signedIn ? window.accountName.charAt(0).toUpperCase() : "M"; logo: !window.signedIn; Layout.leftMargin: 4 }
                        ColumnLayout {
                            spacing: 1; Layout.fillWidth: true
                            Label { text: window.signedIn ? window.accountName : "Not signed in"; color: window.ink; font.pixelSize: 13; elide: Text.ElideRight; Layout.fillWidth: true }
                            Caption { text: workspace.connected ? (workspace.modelStatus === "failing" ? "Model not working" : workspace.modelStatus === "missing" ? "Model not set up" : "Connected") : workspace.mockSession ? "Mock session" : "Signed out"; font.pixelSize: 11; elide: Text.ElideRight; wrapMode: Text.NoWrap; Layout.fillWidth: true }
                        }
                        Icon { path: window.icons.chevron; size: 14 }
                    }
                    background: Rectangle { radius: 2; color: accountChip.hovered ? "#0b2015" : "transparent"; border.color: accountChip.activeFocus ? window.cyan : "transparent" }
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 84; color: "#040b07"
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: window.line }
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: window.compact ? 14 : 22; anchors.rightMargin: window.compact ? 14 : 22; spacing: 14
                    TermCombo { visible: window.compact; model: window.pages; currentIndex: window.page; onActivated: window.page = currentIndex; Accessible.name: "Workspace screen"; Layout.preferredWidth: 150 }
                    ColumnLayout {
                        visible: !window.compact; spacing: 2; Layout.fillWidth: true
                        Label { text: "> " + window.titles[window.page].toUpperCase(); color: window.cyan; font.pixelSize: 22; font.weight: Font.Bold; font.letterSpacing: 2; elide: Text.ElideRight; Layout.fillWidth: true }
                        Caption { text: window.page === 2 ? (workspace.developer.files.length > 0 ? workspace.developer.files.length + " file(s) in this session" : "No project files loaded") : window.descriptions[window.page]; font.pixelSize: 14; elide: Text.ElideRight; wrapMode: Text.NoWrap; Layout.fillWidth: true }
                    }
                    Item { visible: window.compact; Layout.fillWidth: true }
                    RowLayout {
                        visible: !window.compact; spacing: 8
                        Rectangle { implicitWidth: 8; implicitHeight: 8; radius: 4; color: workspace.connected ? (window.modelDown ? "#f5b544" : "#22c55e") : workspace.mockSession ? "#f5b544" : "#2f5a44" }
                        Caption { text: workspace.connected ? (workspace.modelStatus === "failing" ? "Model not working" : workspace.modelStatus === "missing" ? "Model not set up" : "Connected") : workspace.mockSession ? "Mock session" : "Not connected" }
                    }
                    Pill { visible: !window.compact; text: "●  " + workspace.state; tint: workspace.busy ? "#ffb000" : window.cyan }
                    TermCombo {
                        id: mode; model: ["Assistant", "Developer", "Autonomous"]; Layout.preferredWidth: 170; Accessible.name: "Workflow mode"
                        notes: ["Quick answers in conversation.", "Answers as a software engineer, with code and its risks.", "Plans the work, then does each step without asking. Uses several model requests and cannot act on your computer."]
                    }
                }
            }
            ScrollView {
                id: scroll
                Layout.fillWidth: true; Layout.fillHeight: true
                contentWidth: availableWidth
                Component.onCompleted: { const view = contentItem as Flickable; if (view) { view.boundsBehavior = Flickable.StopAtBounds; view.pixelAligned = true } }
                // Always visible and wide enough to grab whenever the page is taller than the view.
                ScrollBar.vertical.policy: scroll.contentHeight > scroll.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                ScrollBar.vertical.width: 12
                ScrollBar.vertical.minimumSize: 0.08
                clip: true
                ColumnLayout {
                    id: content
                    readonly property int gutter: window.compact ? 14 : 22
                    width: scroll.availableWidth
                    spacing: 16
                    WheelHandler {
                        target: null
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                        onWheel: event => window.wheelScroll(event)
                    }
                    Item { implicitHeight: 2 }
                    Card {
                        Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter
                        implicitHeight: statusText.implicitHeight + 24
                        color: "#071209"; border.color: "#1f5a3c"
                        Caption { id: statusText; anchors.fill: parent; anchors.margins: 12; color: workspace.connected && window.modelDown ? "#f5d28a" : "#9be7b8"; text: workspace.connected && workspace.modelStatus === "failing" ? "Signed in, but the model's provider is refusing requests, so the assistant cannot answer. " + workspace.modelProblem + " Memory, skills and rules still save to your account." : workspace.connected && workspace.modelStatus === "missing" ? "Signed in, but no model is set up yet, so the assistant cannot answer. Set a model provider in your Supabase project secrets: ANTHROPIC_API_KEY, or MNAKA_LLM_BASE_URL with MNAKA_LLM_MODEL. Memory, skills and rules still save to your account." : workspace.connected ? "Connected to " + window.modelLabel + ". Messages go to the model set on your account gateway. Conversations, memories, skills and rules are saved to your account as plain text." : workspace.mockSession ? "MOCK SESSION. Replies come from a deterministic fixture, not a reasoning model. Tasks run through the broker's fixture tools only: no files, applications or networks are touched, and nothing is saved when you close MNAKA." : (workspace.accountConfigured ? "Signed out. Sign in under Settings to use the assistant. Nothing is sent or saved while you are signed out." : "No account service is configured for this build, so the assistant is unavailable.") }
                    }
                    GridLayout {
                        visible: window.page === 0
                        Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter
                        columns: window.wide ? 3 : window.compact ? 1 : 2
                        columnSpacing: 16; rowSpacing: 16
                        Card {
                            Layout.fillWidth: true; Layout.preferredWidth: 440; Layout.minimumWidth: 0; Layout.alignment: Qt.AlignTop
                            Layout.preferredHeight: Math.max(conversation.implicitHeight + 36, window.compact ? 0 : scroll.height - statusText.implicitHeight - 110)
                            ColumnLayout {
                                id: conversation
                                anchors.fill: parent; anchors.margins: 18; spacing: 12
                                RowLayout {
                                    Layout.fillWidth: true
                                    ColumnLayout {
                                        spacing: 2; Layout.fillWidth: true; Layout.minimumWidth: 0
                                        Heading {
                                            id: greetingLine
                                            text: window.greeting + (window.signedIn ? ", " + window.accountName : "")
                                            elide: Text.ElideRight; wrapMode: Text.NoWrap; Layout.fillWidth: true; rightPadding: 18
                                            Rectangle {
                                                id: caret
                                                x: Math.min(greetingLine.contentWidth + 8, greetingLine.width - width); y: 3
                                                width: 10; height: greetingLine.height - 6; color: window.cyan
                                                Timer { interval: 550; running: window.page === 0; repeat: true; onTriggered: caret.opacity = caret.opacity > 0.5 ? 0 : 1 }
                                            }
                                        }
                                        Caption { text: "$ awaiting input"; font.pixelSize: 14 }
                                    }
                                    GhostButton { text: "Clear session"; onClicked: clearDialog.open() }
                                }
                                Repeater {
                                    model: workspace.messages
                                    RowLayout {
                                        id: message
                                        required property var modelData
                                        readonly property bool mine: modelData.role === "user"
                                        Layout.fillWidth: true; spacing: 10
                                        Avatar { letter: window.signedIn ? window.accountName.charAt(0).toUpperCase() : "Y"; logo: !message.mine; Layout.alignment: Qt.AlignTop }
                                        Rectangle {
                                            Layout.fillWidth: true; radius: 2
                                            color: message.mine ? "#0b2a1a" : "#071a10"; border.color: message.mine ? "#1f7a4d" : window.line
                                            implicitHeight: bubble.implicitHeight + 24
                                            ColumnLayout {
                                                id: bubble
                                                anchors.fill: parent; anchors.margins: 12; spacing: 4
                                                Label { text: message.mine ? "YOU" : message.modelData.mock ? "MNAKA · MOCK RESPONSE" : "MNAKA"; color: message.mine ? window.cyan : "#ffb000"; font.pixelSize: 10; font.weight: Font.Bold }
                                                Label { text: message.modelData.text; textFormat: Text.PlainText; color: window.ink; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 14 }
                                            }
                                        }
                                    }
                                }
                                Caption { visible: workspace.busy && workspace.state !== "Awaiting approval"; text: workspace.state === "Cancelling" ? "Cancelling…" : "Preparing response…" }
                                RowLayout {
                                    visible: workspace.state === "Awaiting approval"; Layout.fillWidth: true
                                    Caption { text: "This task is waiting for your approval."; Layout.fillWidth: true }
                                    GhostButton { text: "Review in Tasks"; onClicked: window.page = 1 }
                                }
                                Caption { visible: workspace.streamingText.length > 0; text: workspace.streamingText; Layout.fillWidth: true; color: window.cyan }
                                Caption { visible: workspace.notice.length > 0; text: workspace.notice; Layout.fillWidth: true }
                                Item { Layout.fillHeight: true }
                                Rectangle {
                                    id: composerBox
                                    Layout.fillWidth: true; implicitHeight: Math.min(160, Math.max(56, composer.implicitHeight + 16))
                                    radius: 2; color: "#020a06"; border.color: composer.activeFocus ? window.accent : "#1f5a3c"
                                    RowLayout {
                                        anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 8; spacing: 6
                                        ScrollView {
                                            Layout.fillWidth: true; Layout.fillHeight: true
                                            TextArea {
                                                id: composer
                                                placeholderText: "> ask mnaka_"; placeholderTextColor: window.muted
                                                color: window.ink; wrapMode: TextEdit.Wrap; font.pixelSize: 14
                                                Accessible.name: "Message to MNAKA"
                                                background: Rectangle { color: "transparent" }
                                                Keys.onPressed: event => {
                                                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !(event.modifiers & Qt.ShiftModifier)) {
                                                        if (workspace.submit(text, mode.currentText)) text = ""
                                                        event.accepted = true
                                                    }
                                                }
                                            }
                                        }
                                        Button {
                                            Keys.onReturnPressed: clicked()
                                            Keys.onEnterPressed: clicked()
                                            id: talk
                                            visible: workspace.voice.available
                                            implicitWidth: 40; implicitHeight: 40
                                            Accessible.name: window.voiceActive ? "Stop listening" : "Speak a message"
                                            onClicked: { if (workspace.voice.handsFree) workspace.voice.handsFree = false; else if (window.voiceActive) workspace.voice.stop(); else workspace.voice.start() }
                                            contentItem: Item { Icon { anchors.centerIn: parent; path: window.icons.mic; tint: window.voiceActive ? "#021006" : window.cyan; size: 18 } }
                                            background: Rectangle { radius: 2; color: window.voiceActive ? "#ffb000" : talk.hovered ? "#0b2015" : "transparent"; border.color: talk.activeFocus || !window.voiceActive ? "#1f5a3c" : "#ffb000" }
                                        }
                                        Button {
                                            Keys.onReturnPressed: clicked()
                                            Keys.onEnterPressed: clicked()
                                            id: send
                                            implicitWidth: 40; implicitHeight: 40
                                            enabled: workspace.busy || composer.text.trim().length > 0
                                            Accessible.name: workspace.busy ? "Cancel task" : "Send message"
                                            onClicked: { if (workspace.busy) workspace.cancel(); else if (workspace.submit(composer.text, mode.currentText)) composer.text = "" }
                                            contentItem: Item {
                                                Icon { visible: !workspace.busy; anchors.centerIn: parent; path: window.icons.send; tint: "#021006"; size: 18 }
                                                Rectangle { visible: workspace.busy; anchors.centerIn: parent; width: 12; height: 12; radius: 2; color: "#021006" }
                                            }
                                            background: Rectangle { radius: 2; color: !send.enabled ? "#123322" : send.hovered ? "#5dffa6" : window.accent; border.color: send.activeFocus ? window.cyan : "transparent" }
                                        }
                                    }
                                }
                                Caption { text: "Enter to send · Shift+Enter for a new line"; font.pixelSize: 11 }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.preferredWidth: 540; Layout.minimumWidth: 0; Layout.alignment: Qt.AlignTop; spacing: 16
                            Card {
                                Layout.fillWidth: true; Layout.fillHeight: true; implicitHeight: orbColumn.implicitHeight + 44
                                gradient: Gradient { GradientStop { position: 0; color: "#030a06" } GradientStop { position: 1; color: "#071a10" } }
                                ColumnLayout {
                                    id: orbColumn
                                    anchors.centerIn: parent; width: parent.width - 40; spacing: 6
                                    Orb { Layout.alignment: Qt.AlignHCenter; active: workspace.busy || window.voiceActive || workspace.voice.partial.length > 0; size: window.compact ? 170 : 230 }
                                    Label { Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 6; text: window.voiceActive ? "Listening…" : workspace.busy ? workspace.state : window.voiceWaiting ? "Say \"Mnaka\"" : workspace.connected ? (workspace.modelStatus === "failing" ? "Model not working" : workspace.modelStatus === "missing" ? "Model not set up" : workspace.voice.available ? "Ready to listen" : "Ready") : workspace.mockSession ? "Mock session ready" : "Not connected"; color: window.voiceActive ? "#ffb000" : window.ink; font.pixelSize: 19; font.weight: Font.DemiBold }
                                    Caption {
                                        Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                                        color: workspace.voice.partial.length > 0 ? window.ink : window.muted
                                        text: window.voiceActive ? (workspace.voice.partial.length > 0 ? workspace.voice.partial : workspace.voice.openMicrophone ? "Say your request. Everything heard is sent." : workspace.voice.handsFree ? "Yes? Say your request." : "Speak now. Press the microphone again to finish.")
                                            : window.voiceWaiting && workspace.voice.partial.length > 0 ? workspace.voice.partial
                                            : window.voiceWaiting && workspace.voice.notice.length === 0 && window.voiceNotice.length === 0 && !workspace.busy ? "Hands-free is on. Say \"Mnaka\" and then your request, or press the microphone to keep it listening all the time."
                                            : workspace.voice.notice.length > 0 ? workspace.voice.notice
                                            : window.voiceNotice.length > 0 ? window.voiceNotice
                                            : workspace.state === "Awaiting approval" ? "Review the requested action under Approval required."
                                            : workspace.busy ? "Working on your request."
                                            : workspace.voice.available ? (workspace.connected ? "Press the microphone and speak, or type a message." : "Sign in under Settings to connect. You can still say \"open settings\".")
                                            : workspace.connected ? "Type a message to talk to the model. Voice input needs a browser with speech recognition." : workspace.mockSession ? "Type a message to run a fixture task." : "Sign in under Settings to connect."
                                    }
                                    Button {
                                        Keys.onReturnPressed: clicked()
                                        Keys.onEnterPressed: clicked()
                                        id: orbTalk
                                        visible: workspace.voice.available
                                        Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 10
                                        implicitWidth: 56; implicitHeight: 56
                                        Accessible.name: window.voiceActive ? "Stop listening" : "Start listening"
                                        onClicked: { if (workspace.voice.handsFree) workspace.voice.handsFree = false; else if (window.voiceActive) workspace.voice.stop(); else workspace.voice.start() }
                                        contentItem: Item { Icon { anchors.centerIn: parent; path: window.icons.mic; tint: window.voiceActive ? "#021006" : window.cyan; size: 24 } }
                                        background: Rectangle { radius: 28; color: window.voiceActive ? "#ffb000" : orbTalk.hovered ? "#0b2015" : "#020a06"; border.color: window.voiceActive ? "#ffb000" : orbTalk.activeFocus ? window.ink : window.cyan; border.width: 2 }
                                    }
                                    Switch {
                                        visible: workspace.voice.available
                                        Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 6
                                        text: "Hands-free: always listening"
                                        checked: workspace.voice.handsFree
                                        Accessible.name: "Hands-free, always listening"
                                        onToggled: workspace.voice.handsFree = checked
                                    }
                                    Caption {
                                        visible: workspace.voice.available; Layout.fillWidth: true; Layout.topMargin: 4; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 11
                                        color: workspace.voice.handsFree ? "#f5d28a" : window.muted
                                        text: workspace.voice.handsFree
                                            ? (workspace.voice.openMicrophone ? "Always listening: everything the browser hears is taken as a request and sent to the model, without \"Mnaka\" in front. That includes other people, a television or a call. Press the microphone or this switch to stop." : "Hands-free keeps the microphone on while this page is open. The browser hears everything said nearby and, in Chrome and Edge, sends that audio to the browser vendor's speech service. MNAKA acts only on speech that starts with \"Mnaka\" and discards the rest. Turn it off when you are not using it.")
                                            : "Speech is recognised by this browser. In Chrome and Edge the audio goes to the browser vendor's speech service; MNAKA and the model receive only the text."
                                    }
                                }
                            }
                            GridLayout {
                                columns: window.compact ? 1 : window.width >= 1500 ? 3 : 2; columnSpacing: 12; rowSpacing: 12; Layout.fillWidth: true; Layout.fillHeight: false
                                uniformCellWidths: true
                                EngineCard { Layout.fillWidth: true; Layout.fillHeight: true; icon: window.icons.code; name: "Software Engineering"; engineId: "developer"; destination: 2 }
                                EngineCard { Layout.fillWidth: true; Layout.fillHeight: true; icon: window.icons.monitor; name: "Computer Control"; engineId: "control"; destination: 3 }
                                EngineCard { Layout.fillWidth: true; Layout.fillHeight: true; icon: window.icons.layers; name: "Self-extending Skills"; engineId: "skills"; destination: 4 }
                                EngineCard { Layout.fillWidth: true; Layout.fillHeight: true; icon: window.icons.chart; name: "Project Intelligence"; engineId: "memory"; destination: 5 }
                                EngineCard { Layout.fillWidth: true; Layout.fillHeight: true; icon: window.icons.eye; name: "Vision & Spatial"; engineId: "vision"; destination: 6 }
                                EngineCard { Layout.fillWidth: true; Layout.fillHeight: true; icon: window.icons.zap; name: "Proactive Intelligence"; engineId: "proactive"; destination: 7 }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.fillHeight: false; Layout.preferredWidth: 330; Layout.minimumWidth: 0; Layout.alignment: Qt.AlignTop; spacing: 16
                            Layout.columnSpan: !window.wide && !window.compact ? 2 : 1
                            Card {
                                Layout.fillWidth: true; implicitHeight: activeColumn.implicitHeight + 36
                                ColumnLayout {
                                    id: activeColumn
                                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 18; spacing: 10
                                    RowLayout {
                                        Layout.fillWidth: true
                                        SectionTitle { text: "Active tasks"; count: window.activeTaskCount(); Layout.fillWidth: true }
                                        GhostButton { text: "See all"; implicitHeight: 30; onClicked: window.page = 1 }
                                    }
                                    Caption { visible: workspace.tasks.length === 0; text: (workspace.mockSession || workspace.connected) ? "No tasks yet. Send a message to create one." : "No tasks. Sign in to submit requests."; Layout.fillWidth: true }
                                    Repeater {
                                        model: workspace.tasks.slice(-3).reverse()
                                        Rectangle {
                                            id: recent
                                            required property var modelData
                                            Layout.fillWidth: true; radius: 2; color: "#020a06"; border.color: window.line
                                            implicitHeight: recentColumn.implicitHeight + 24
                                            ColumnLayout {
                                                id: recentColumn
                                                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 12; spacing: 6
                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    Label { text: recent.modelData.title; textFormat: Text.PlainText; color: window.ink; font.pixelSize: 13; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                                                    Pill { text: recent.modelData.state; tint: recent.modelData.state === "succeeded" ? "#4ade80" : recent.modelData.active ? "#ffb000" : window.muted }
                                                }
                                                Caption { text: recent.modelData.evidence; font.pixelSize: 12; Layout.fillWidth: true }
                                            }
                                        }
                                    }
                                }
                            }
                            Card {
                                Layout.fillWidth: true; implicitHeight: approvalBrief.implicitHeight + 36
                                border.color: workspace.approval.id !== undefined ? "#ffb000" : window.line
                                ColumnLayout {
                                    id: approvalBrief
                                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 18; spacing: 10
                                    SectionTitle { icon: window.icons.bell; text: "Approval required"; count: workspace.approval.id !== undefined ? 1 : 0; Layout.fillWidth: true }
                                    Caption { visible: workspace.approval.id === undefined; text: "Nothing is waiting for your approval."; Layout.fillWidth: true }
                                    Label { visible: workspace.approval.id !== undefined; text: workspace.approval.tool || ""; color: window.ink; font.pixelSize: 14; font.weight: Font.DemiBold }
                                    Caption { visible: workspace.approval.id !== undefined; text: (workspace.approval.effect || "") + "\nSimulation only. This approval grants no production permissions."; font.pixelSize: 12; Layout.fillWidth: true }
                                    RowLayout {
                                        visible: workspace.approval.id !== undefined; spacing: 10; Layout.fillWidth: true
                                        PrimaryButton { text: "Review changes"; Layout.fillWidth: true; onClicked: window.page = 1 }
                                        GhostButton { text: "Decline"; onClicked: workspace.decideApproval(false) }
                                    }
                                }
                            }
                            Card {
                                Layout.fillWidth: true; implicitHeight: upcoming.implicitHeight + 36
                                ColumnLayout {
                                    id: upcoming
                                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 18; spacing: 10
                                    RowLayout {
                                        Layout.fillWidth: true
                                        SectionTitle { icon: window.icons.calendar; text: "Upcoming"; Layout.fillWidth: true }
                                        GhostButton { text: "See all"; implicitHeight: 30; onClicked: window.page = 7 }
                                    }
                                    Repeater {
                                        model: workspace.rules.slice(0, 3)
                                        RowLayout {
                                            id: upcomingRule
                                            required property var modelData
                                            Layout.fillWidth: true; spacing: 10
                                            Rectangle { implicitWidth: 3; implicitHeight: 34; radius: 2; color: upcomingRule.modelData.enabled ? window.accent : "#1f5a3c" }
                                            ColumnLayout {
                                                spacing: 1; Layout.fillWidth: true
                                                Label { text: upcomingRule.modelData.title; textFormat: Text.PlainText; color: window.ink; font.pixelSize: 13; elide: Text.ElideRight; Layout.fillWidth: true }
                                                Caption { text: upcomingRule.modelData.schedule + (upcomingRule.modelData.enabled ? "" : " · off"); font.pixelSize: 12; elide: Text.ElideRight; wrapMode: Text.NoWrap; Layout.fillWidth: true }
                                            }
                                        }
                                    }
                                    Caption { text: workspace.rules.length > 0 ? "Saved rules are not scheduled or run yet." : workspace.connected ? "No rules saved. Add one under Proactive; rules are not scheduled or run yet." : "No rules. Sign in to save rules to your account."; Layout.fillWidth: true }
                                }
                            }
                        }
                    }
                    ColumnLayout {
                        visible: window.page === 1; Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter; spacing: 12
                        Card {
                            visible: workspace.approval.id !== undefined
                            Layout.fillWidth: true; implicitHeight: approvalContent.implicitHeight + 36
                            border.color: "#ffb000"
                            ColumnLayout {
                                id: approvalContent; anchors.fill: parent; anchors.margins: 18; spacing: 10
                                Heading { text: "Review this action"; Layout.fillWidth: true }
                                Caption { text: "Simulation only. This approval grants no production permissions."; Layout.fillWidth: true }
                                Label { text: "Tool: " + (workspace.approval.tool || ""); color: window.cyan }
                                Caption { text: "Destination: " + (workspace.approval.destination || "") + "\nEffect: " + (workspace.approval.effect || "") + "\nData leaving device: " + (workspace.approval.dataLeavingDevice || "") + "\nExpires: " + (workspace.approval.expiresAt || ""); Layout.fillWidth: true }
                                Caption { text: "Intent hash: " + (workspace.approval.intentHash || ""); Layout.fillWidth: true; font.pixelSize: 10 }
                                RowLayout {
                                    spacing: 10
                                    PrimaryButton { text: "Approve action"; Accessible.name: "Approve requested action"; onClicked: workspace.decideApproval(true) }
                                    GhostButton { text: "Deny"; onClicked: workspace.decideApproval(false) }
                                }
                            }
                        }
                        Caption { visible: workspace.tasks.length === 0; text: (workspace.mockSession || workspace.connected) ? "No tasks yet. Send a message from Overview to create one." : "No tasks. Sign in to submit requests."; Layout.fillWidth: true }
                        Repeater {
                            model: workspace.tasks
                            Card {
                                id: taskCard
                                required property var modelData
                                Layout.fillWidth: true; implicitHeight: taskContent.implicitHeight + 36
                                ColumnLayout {
                                    id: taskContent; anchors.fill: parent; anchors.margins: 18; spacing: 8
                                    RowLayout { Layout.fillWidth: true; Label { text: taskCard.modelData.mode; color: window.muted } Item { Layout.fillWidth: true } Pill { text: taskCard.modelData.state; tint: taskCard.modelData.state === "succeeded" ? "#4ade80" : window.cyan } }
                                    Label { text: taskCard.modelData.title; textFormat: Text.PlainText; color: window.ink; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 15 }
                                    Caption { text: taskCard.modelData.evidence; Layout.fillWidth: true }
                                    Caption { visible: taskCard.modelData.plannedTool !== undefined; text: "Validated plan: " + (taskCard.modelData.plannedTool || ""); Layout.fillWidth: true }
                                    Caption { text: taskCard.modelData.id + " · " + taskCard.modelData.createdAt; Layout.fillWidth: true; font.pixelSize: 10 }
                                    GhostButton { visible: taskCard.modelData.active; text: "Cancel task"; onClicked: workspace.cancel() }
                                }
                            }
                        }
                        Label { visible: workspace.events.length > 0; text: "SESSION EVENTS · NOT PERSISTED"; color: window.muted; font.pixelSize: 11; font.letterSpacing: 2; Layout.topMargin: 10 }
                        Repeater {
                            model: workspace.events
                            Caption {
                                required property var modelData
                                Layout.fillWidth: true
                                text: "#" + modelData.sequence + "  " + modelData.type + "  " + (modelData.payload.new_state || modelData.payload.summary || "")
                                font.pixelSize: 11
                            }
                        }
                    }
                    GridLayout {
                        id: dev
                        readonly property var session: workspace.developer
                        visible: window.page === 2
                        Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter
                        columns: window.wide ? 3 : window.compact ? 1 : 2
                        columnSpacing: 16; rowSpacing: 16
                        Card {
                            Layout.fillWidth: true; Layout.fillHeight: !window.compact; Layout.preferredWidth: 260; Layout.minimumWidth: 0; Layout.alignment: Qt.AlignTop
                            implicitHeight: treeColumn.implicitHeight + 32
                            ColumnLayout {
                                id: treeColumn
                                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16; spacing: 6
                                SectionTitle { text: "Project"; count: dev.session.files.length; Layout.fillWidth: true }
                                RowLayout {
                                    spacing: 8; Layout.bottomMargin: 4
                                    PrimaryButton { text: "Add files"; implicitHeight: 32; enabled: !dev.session.busy; onClicked: dev.session.browserFiles ? dev.session.pickFiles() : projectPicker.open() }
                                    GhostButton { text: "Clear"; implicitHeight: 32; enabled: dev.session.files.length > 0 && !dev.session.busy; onClicked: dev.session.clearProject() }
                                }
                                Caption { visible: dev.session.files.length === 0; Layout.fillWidth: true; text: "Add the text files a change concerns: up to 24 files, 64 KiB each. They stay in this session until you ask for a change; then they are sent to the model." }
                                Repeater {
                                    model: dev.session.files
                                    Rectangle {
                                        id: node
                                        required property var modelData
                                        Layout.fillWidth: true; implicitHeight: 34; radius: 2
                                        color: node.modelData.proposed ? "#0b2416" : "transparent"
                                        RowLayout {
                                            anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 4; spacing: 8
                                            Icon { path: window.icons.file; size: 16; tint: window.cyan }
                                            Label { text: node.modelData.path; textFormat: Text.PlainText; color: node.modelData.proposed || node.modelData.unsaved ? window.ink : window.muted; font.pixelSize: 13; elide: Text.ElideMiddle; Layout.fillWidth: true }
                                            Pill { visible: node.modelData.isNew === true; text: "NEW"; tint: "#f5b544"; implicitHeight: 20 }
                                            Rectangle { visible: node.modelData.proposed && node.modelData.isNew !== true; implicitWidth: 7; implicitHeight: 7; radius: 4; color: "#f5b544"; Accessible.name: "Has proposed changes" }
                                            GhostButton {
                                                visible: node.modelData.unsaved; text: "Save"; implicitHeight: 26; leftPadding: 8; rightPadding: 8
                                                Accessible.name: "Save " + node.modelData.path
                                                onClicked: { if (dev.session.browserFiles) dev.session.saveFile(node.modelData.path); else { projectSaver.path = node.modelData.path; projectSaver.open() } }
                                            }
                                            GhostButton {
                                                visible: node.modelData.isNew !== true; text: "×"; implicitHeight: 26; leftPadding: 8; rightPadding: 8
                                                enabled: !dev.session.busy; Accessible.name: "Remove " + node.modelData.path
                                                onClicked: dev.session.removeFile(node.modelData.path)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        Card {
                            Layout.fillWidth: true; Layout.preferredWidth: 620; Layout.minimumWidth: 0; Layout.alignment: Qt.AlignTop
                            implicitHeight: diffColumn.implicitHeight + 20
                            ColumnLayout {
                                id: diffColumn
                                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 1; spacing: 0
                                RowLayout {
                                    spacing: 0; Layout.fillWidth: true
                                    Repeater {
                                        model: ["Diff", "Build output", "Tests"]
                                        Button {
                                            Keys.onReturnPressed: clicked()
                                            Keys.onEnterPressed: clicked()
                                            id: tab
                                            required property int index
                                            required property string modelData
                                            text: modelData; implicitHeight: 44; leftPadding: 22; rightPadding: 22
                                            checkable: true; checked: window.devTab === index
                                            onClicked: window.devTab = index
                                            contentItem: Label { text: tab.text; color: tab.checked ? "#00ff9c" : window.muted; font.pixelSize: 13; font.weight: tab.checked ? Font.DemiBold : Font.Normal; verticalAlignment: Text.AlignVCenter }
                                            background: Item {
                                                Rectangle { anchors.fill: parent; color: tab.hovered ? "#0b2015" : "transparent"; border.color: tab.activeFocus ? window.cyan : "transparent" }
                                                Rectangle { visible: tab.checked; anchors.bottom: parent.bottom; width: parent.width; height: 2; color: window.accent }
                                            }
                                        }
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: window.line }
                                ColumnLayout {
                                    visible: window.devTab === 0
                                    Layout.fillWidth: true; Layout.margins: 16; spacing: 12
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label { text: "Proposed changes"; color: window.ink; font.pixelSize: 17; font.weight: Font.DemiBold; Layout.fillWidth: true }
                                        Caption { visible: dev.session.hasProposal; text: dev.session.proposal.length + " file(s) changed" }
                                    }
                                    Caption {
                                        visible: !dev.session.hasProposal; Layout.fillWidth: true
                                        text: dev.session.busy ? "Waiting for the model…" : dev.session.files.length === 0 ? "No proposed changes. Add project files, then describe the change you want below."
                                            : "No proposed changes. Describe the change you want below; the model replies with edited files and the difference appears here."
                                    }
                                    Repeater {
                                        model: dev.session.proposal
                                        Rectangle {
                                            id: fileDiff
                                            required property var modelData
                                            property bool expanded: true
                                            Layout.fillWidth: true; radius: 2; color: "#020a06"; border.color: window.line; clip: true
                                            implicitHeight: fileColumn.implicitHeight + 2
                                            ColumnLayout {
                                                id: fileColumn
                                                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 1; spacing: 0
                                                Button {
                                                    Keys.onReturnPressed: clicked()
                                                    Keys.onEnterPressed: clicked()
                                                    id: fileHeader
                                                    Layout.fillWidth: true; implicitHeight: 40
                                                    Accessible.name: (fileDiff.expanded ? "Collapse " : "Expand ") + fileDiff.modelData.path
                                                    onClicked: fileDiff.expanded = !fileDiff.expanded
                                                    contentItem: RowLayout {
                                                        spacing: 10
                                                        Icon { path: fileDiff.expanded ? window.icons.down : window.icons.chevron; size: 16; Layout.leftMargin: 10 }
                                                        Icon { path: window.icons.file; size: 16; tint: window.cyan }
                                                        Label { text: fileDiff.modelData.path + (fileDiff.modelData.isNew ? "  (new file)" : ""); textFormat: Text.PlainText; color: window.ink; font.pixelSize: 13; elide: Text.ElideMiddle; Layout.fillWidth: true }
                                                        Label { text: "+" + fileDiff.modelData.added; color: "#4ade80"; font.pixelSize: 13 }
                                                        Label { text: "\u2212" + fileDiff.modelData.removed; color: "#f87171"; font.pixelSize: 13; Layout.rightMargin: 14 }
                                                    }
                                                    background: Rectangle { color: fileHeader.hovered ? "#0b2015" : "transparent"; border.color: fileHeader.activeFocus ? window.cyan : "transparent" }
                                                }
                                                Repeater {
                                                    model: fileDiff.expanded ? fileDiff.modelData.hunks : []
                                                    ColumnLayout {
                                                        id: hunk
                                                        required property var modelData
                                                        Layout.fillWidth: true; spacing: 0
                                                        Rectangle {
                                                            Layout.fillWidth: true; implicitHeight: 24; color: "#082015"
                                                            Label { anchors.verticalCenter: parent.verticalCenter; x: 70; text: hunk.modelData.header; color: "#4dd8a0"; font.pixelSize: 12 }
                                                        }
                                                        Repeater {
                                                            model: hunk.modelData.lines
                                                            DiffLine {}
                                                        }
                                                    }
                                                }
                                                Caption { visible: fileDiff.expanded && fileDiff.modelData.truncated; text: "Only the first 800 changed lines are shown. Applying uses the whole file."; Layout.margins: 10 }
                                                Item { visible: fileDiff.expanded; implicitHeight: 6 }
                                            }
                                        }
                                    }
                                }
                                Caption {
                                    visible: window.devTab !== 0
                                    Layout.fillWidth: true; Layout.margins: 16
                                    text: window.devTab === 1 ? "No build output. Nothing has been built: builds need a qualified isolated sandbox, which is not available."
                                        : "No test results. Nothing has been run: project tests need the same sandbox as builds."
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.fillHeight: false; Layout.preferredWidth: 340; Layout.minimumWidth: 0; Layout.alignment: Qt.AlignTop; spacing: 16
                            Layout.columnSpan: !window.wide && !window.compact ? 2 : 1
                            Card {
                                Layout.fillWidth: true; implicitHeight: checklist.implicitHeight + 36
                                ColumnLayout {
                                    id: checklist
                                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 18; spacing: 12
                                    SectionTitle { text: "Task checklist"; Layout.fillWidth: true }
                                    Repeater {
                                        model: dev.session.steps
                                        RowLayout {
                                            id: step
                                            required property var modelData
                                            readonly property string state: modelData.state
                                            Layout.fillWidth: true; spacing: 12
                                            Rectangle {
                                                Layout.alignment: Qt.AlignTop
                                                implicitWidth: 24; implicitHeight: 24; radius: 2
                                                color: step.state === "done" ? "#22c55e" : "transparent"
                                                border.color: step.state === "done" ? "#22c55e" : step.state === "active" ? window.cyan : step.state === "skipped" ? "#f5b544" : "#1f5a3c"; border.width: 2
                                                Icon { visible: step.state === "done"; anchors.centerIn: parent; path: window.icons.check; size: 14; weight: 2.4; tint: "#021006" }
                                                Label { visible: step.state === "skipped"; anchors.centerIn: parent; text: "–"; color: "#f5b544"; font.weight: Font.Bold }
                                            }
                                            ColumnLayout {
                                                spacing: 2; Layout.fillWidth: true
                                                Label { text: step.modelData.title; color: step.state === "pending" ? window.muted : window.ink; font.pixelSize: 14; font.weight: Font.DemiBold }
                                                Caption { text: step.modelData.detail; Layout.fillWidth: true }
                                            }
                                        }
                                    }
                                }
                            }
                            Card {
                                visible: dev.session.hasProposal || dev.session.summary.length > 0
                                Layout.fillWidth: true; implicitHeight: review.implicitHeight + 36
                                ColumnLayout {
                                    id: review
                                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 18; spacing: 12
                                    SectionTitle { text: "Review before applying"; Layout.fillWidth: true }
                                    Caption { text: "YOUR MODEL'S SUMMARY, NOT VERIFIED"; font.pixelSize: 10; font.letterSpacing: 1; color: "#f5b544" }
                                    Label { Layout.fillWidth: true; text: dev.session.summary.length > 0 ? dev.session.summary : "The model gave no summary."; textFormat: Text.PlainText; color: window.ink; wrapMode: Text.WordWrap; font.pixelSize: 14 }
                                    PrimaryButton { text: "Apply changes"; enabled: dev.session.hasProposal && !dev.session.busy; Layout.fillWidth: true; implicitHeight: 44; onClicked: dev.session.applyProposal() }
                                    GhostButton { text: "Discard"; enabled: dev.session.hasProposal && !dev.session.busy; Layout.fillWidth: true; onClicked: dev.session.discardProposal() }
                                    Caption { Layout.fillWidth: true; font.pixelSize: 12; text: "Applying updates the copies in this session. Nothing on your device changes until you save a file." }
                                }
                            }
                            Card {
                                visible: dev.session.hasProposal
                                Layout.fillWidth: true; implicitHeight: summary.implicitHeight + 36
                                ColumnLayout {
                                    id: summary
                                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 18; spacing: 12
                                    SectionTitle { text: "Summary"; Layout.fillWidth: true }
                                    RowLayout {
                                        Layout.fillWidth: true; spacing: 10
                                        Icon { path: window.icons.file; size: 18 }
                                        Label { text: dev.session.proposal.length + " file(s) changed"; color: window.ink; font.pixelSize: 14; Layout.fillWidth: true }
                                        Label { text: "+" + dev.session.added; color: "#4ade80"; font.pixelSize: 14 }
                                        Label { text: "\u2212" + dev.session.removed; color: "#f87171"; font.pixelSize: 14 }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true; spacing: 10
                                        Icon { path: window.icons.info; size: 18; tint: "#f5b544"; Layout.alignment: Qt.AlignTop }
                                        Caption { text: "No build or test has run on these changes."; Layout.fillWidth: true }
                                    }
                                }
                            }
                        }
                    }
                    Card {
                        visible: window.page === 2
                        Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter
                        implicitHeight: promptColumn.implicitHeight + 24
                        ColumnLayout {
                            id: promptColumn
                            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 12; spacing: 8
                            Caption { visible: text.length > 0; Layout.fillWidth: true; color: "#f5d28a"; text: workspace.developer.notice }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 10
                                Field {
                                    id: devPrompt
                                    Layout.fillWidth: true; implicitHeight: 44; maximumLength: 4000
                                    placeholderText: "> describe the change you want_"
                                    Accessible.name: "Change request"
                                    onAccepted: { workspace.developer.request(text); if (workspace.developer.busy) clear() }
                                }
                                Button {
                                    Keys.onReturnPressed: clicked()
                                    Keys.onEnterPressed: clicked()
                                    id: devSend
                                    implicitWidth: 44; implicitHeight: 44
                                    enabled: !workspace.developer.busy && devPrompt.text.trim().length > 0
                                    Accessible.name: "Send change request"
                                    onClicked: { workspace.developer.request(devPrompt.text); if (workspace.developer.busy) devPrompt.clear() }
                                    contentItem: Item { Icon { anchors.centerIn: parent; path: window.icons.send; tint: "#021006"; size: 18 } }
                                    background: Rectangle { radius: 2; color: !devSend.enabled ? "#123322" : devSend.hovered ? "#5dffa6" : window.accent; border.color: devSend.activeFocus ? window.cyan : "transparent" }
                                }
                            }
                            Caption { Layout.fillWidth: true; font.pixelSize: 11; text: workspace.connected ? "Sending a request sends every file in the project to the model through your account gateway. File contents are not stored; the request and the model's summary are added to your conversation." : "Sign in under Settings to ask for a change. Files you add stay in this session." }
                        }
                    }
                    Card {
                        visible: window.page === 3 || (window.page >= 4 && window.page <= 7 && !workspace.connected)
                        Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter
                        implicitHeight: engineContent.implicitHeight + 48
                        ColumnLayout {
                            id: engineContent; anchors.fill: parent; anchors.margins: 24; spacing: 14
                            RowLayout {
                                spacing: 12
                                Icon { path: window.pageIcons[window.page]; tint: window.cyan; size: 26 }
                                Pill { text: "NOT CONNECTED"; tint: window.muted }
                            }
                            Heading { Layout.fillWidth: true; text: ["", "", "A reviewable change starts with a trusted project", "No desktop is paired", "No skills are installed", "No persistent projects or memories", "Vision adapters are not configured", "No proactive rules are active"][window.page] || "" }
                            Caption {
                                Layout.fillWidth: true
                                text: ["", "", "Project picker, indexing, diff, build logs and rollback require the project broker and PostgreSQL. Builds additionally require a tested OS sandbox. Native Windows execution remains disabled.", "No paired devices or application adapters are connected.", "Skills will use registered tools only. Installation requires schema validation, validation tests, a pinned manifest hash and explicit authorization. Generated workflows cannot alter host permissions.", "Facts and decisions will include source, scope, confidence and status. PostgreSQL is the only relational store. No fallback database is created. Export, correction and deletion require the storage service.", "Select a local PNG or JPEG image to preview it. Image interpretation and 3D asset loading are unavailable.", "Planned first rule: weekdays at 08:00, check the recorded build result and suggest a notification on failure. Scheduling needs timezone, quiet hours, cooldown and authorized tool scope."][window.page] || ""
                            }
                            GhostButton { text: "Back to overview"; onClicked: window.page = 0 }
                        }
                    }
                    Card {
                        visible: window.page === 5 && workspace.connected
                        Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter
                        implicitHeight: memoryColumn.implicitHeight + 48
                        ColumnLayout {
                            id: memoryColumn
                            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 24; spacing: 12
                            Heading { text: "Memory" }
                            Caption { Layout.fillWidth: true; text: "Facts you save here are sent to the model with every message, and stored in your account as plain text. Projects and indexing need the desktop app." }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 10
                                Field {
                                    id: memoryField
                                    Layout.fillWidth: true; placeholderText: "Something MNAKA should remember"
                                    Accessible.name: "New memory"; maximumLength: 2000
                                    onAccepted: { workspace.addMemory(text); clear() }
                                }
                                PrimaryButton { text: "Remember"; enabled: memoryField.text.trim().length > 0; onClicked: { workspace.addMemory(memoryField.text); memoryField.clear() } }
                            }
                            Caption { text: workspace.dataNotice; visible: text.length > 0; Layout.fillWidth: true; color: "#f5d28a" }
                            Caption { visible: workspace.memories.length === 0; text: "Nothing saved yet."; Layout.fillWidth: true }
                            Repeater {
                                model: workspace.memories
                                Rectangle {
                                    id: memoryRow
                                    required property var modelData
                                    Layout.fillWidth: true; radius: 2; color: "#020a06"; border.color: window.line
                                    implicitHeight: memoryRowLayout.implicitHeight + 20
                                    RowLayout {
                                        id: memoryRowLayout
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.margins: 12; spacing: 12
                                        Label { text: memoryRow.modelData.content; textFormat: Text.PlainText; color: window.ink; wrapMode: Text.WordWrap; font.pixelSize: 14; Layout.fillWidth: true }
                                        GhostButton { text: "Forget"; implicitHeight: 32; Accessible.name: "Forget this memory"; onClicked: workspace.deleteMemory(memoryRow.modelData.id) }
                                    }
                                }
                            }
                        }
                    }
                    Card {
                        visible: window.page === 7 && workspace.connected
                        Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter
                        implicitHeight: rulesColumn.implicitHeight + 48
                        ColumnLayout {
                            id: rulesColumn
                            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 24; spacing: 12
                            Heading { text: "Proactive rules" }
                            Caption { Layout.fillWidth: true; text: "Rules are saved to your account. Nothing schedules or runs them yet: turning a rule on records your choice only." }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 10
                                Field { id: ruleTitle; Layout.fillWidth: true; Layout.preferredWidth: 300; placeholderText: "What to do, e.g. Check the build result"; Accessible.name: "Rule title"; maximumLength: 200 }
                                Field { id: ruleSchedule; Layout.fillWidth: true; Layout.preferredWidth: 200; placeholderText: "When, e.g. Weekdays 08:00"; Accessible.name: "Rule schedule"; maximumLength: 200 }
                                PrimaryButton {
                                    text: "Add rule"; enabled: ruleTitle.text.trim().length > 0 && ruleSchedule.text.trim().length > 0
                                    onClicked: { workspace.addRule(ruleTitle.text, ruleSchedule.text); ruleTitle.clear(); ruleSchedule.clear() }
                                }
                            }
                            Caption { text: workspace.dataNotice; visible: text.length > 0; Layout.fillWidth: true; color: "#f5d28a" }
                            Caption { visible: workspace.rules.length === 0; text: "No rules saved."; Layout.fillWidth: true }
                            Repeater {
                                model: workspace.rules
                                Rectangle {
                                    id: ruleRow
                                    required property var modelData
                                    Layout.fillWidth: true; radius: 2; color: "#020a06"; border.color: window.line
                                    implicitHeight: ruleRowLayout.implicitHeight + 16
                                    RowLayout {
                                        id: ruleRowLayout
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.margins: 12; spacing: 12
                                        ColumnLayout {
                                            spacing: 2; Layout.fillWidth: true
                                            Label { text: ruleRow.modelData.title; textFormat: Text.PlainText; color: window.ink; wrapMode: Text.WordWrap; font.pixelSize: 14; Layout.fillWidth: true }
                                            Caption { text: ruleRow.modelData.schedule; Layout.fillWidth: true }
                                        }
                                        Switch { checked: ruleRow.modelData.enabled; Accessible.name: "Rule enabled"; onToggled: workspace.setRuleEnabled(ruleRow.modelData.id, checked) }
                                        GhostButton { text: "Delete"; implicitHeight: 32; Accessible.name: "Delete this rule"; onClicked: workspace.deleteRule(ruleRow.modelData.id) }
                                    }
                                }
                            }
                        }
                    }
                    Card {
                        visible: window.page === 4; Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter; implicitHeight: skillContent.implicitHeight + 48
                        ColumnLayout {
                            id: skillContent
                            anchors.fill: parent; anchors.margins: 24; spacing: 12
                            Heading { text: "Inspect a skill manifest" }
                            Caption { text: "Paste your JSON manifest. Checks run locally; inspection grants no permissions."; Layout.fillWidth: true }
                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: 240
                                radius: 2; color: "#020a06"; border.color: skillEditor.activeFocus ? window.accent : "#1f5a3c"
                                ScrollView {
                                    anchors.fill: parent; anchors.margins: 4
                                    TextArea {
                                        id: skillEditor
                                        placeholderText: "Skill manifest JSON"; placeholderTextColor: window.muted
                                        color: window.ink; wrapMode: TextEdit.Wrap
                                        selectByMouse: true
                                        Accessible.name: "Skill manifest JSON"
                                        background: Rectangle { color: "transparent" }
                                        onTextChanged: workspace.clearSkillReport()
                                    }
                                }
                            }
                            RowLayout {
                                spacing: 10
                                PrimaryButton { text: "Validate manifest"; enabled: skillEditor.text.trim().length > 0; onClicked: workspace.inspectSkill(skillEditor.text) }
                                GhostButton { text: "Clear"; onClicked: { skillEditor.clear(); workspace.clearSkillReport() } }
                                PrimaryButton { visible: workspace.skillSavable; text: "Save to account"; onClicked: workspace.saveSkill() }
                            }
                            Caption { text: workspace.skillReport; visible: text.length > 0; Layout.fillWidth: true }
                            Caption { visible: workspace.connected && window.page === 4; text: workspace.dataNotice.length > 0 ? workspace.dataNotice : "A manifest that passes inspection can be saved to your account. Saving grants no permission and nothing is executed."; Layout.fillWidth: true; color: workspace.dataNotice.length > 0 ? "#f5d28a" : window.muted }
                            Label { visible: workspace.skills.length > 0; text: "SAVED MANIFESTS"; color: window.muted; font.pixelSize: 11; font.letterSpacing: 2; Layout.topMargin: 6 }
                            Repeater {
                                model: workspace.skills
                                Rectangle {
                                    id: skillRow
                                    required property var modelData
                                    Layout.fillWidth: true; radius: 2; color: "#020a06"; border.color: window.line
                                    implicitHeight: skillRowLayout.implicitHeight + 16
                                    RowLayout {
                                        id: skillRowLayout
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.margins: 12; spacing: 12
                                        ColumnLayout {
                                            spacing: 2; Layout.fillWidth: true
                                            Label { text: skillRow.modelData.skill_id + " · " + skillRow.modelData.version; textFormat: Text.PlainText; color: window.ink; font.pixelSize: 14; elide: Text.ElideRight; Layout.fillWidth: true }
                                            Caption { text: skillRow.modelData.artifact_hash; font.family: window.mono; font.pixelSize: 11; elide: Text.ElideMiddle; wrapMode: Text.NoWrap; Layout.fillWidth: true }
                                        }
                                        GhostButton { text: "Delete"; implicitHeight: 32; Accessible.name: "Delete this saved manifest"; onClicked: workspace.deleteSkill(skillRow.modelData.id) }
                                    }
                                }
                            }
                        }
                    }
                    Card {
                        visible: window.page === 6; Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter; implicitHeight: visionColumn.implicitHeight + 48
                        ColumnLayout {
                            id: visionColumn
                            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 24; spacing: 12
                            RowLayout {
                                spacing: 10
                                PrimaryButton { text: "Select image"; onClicked: workspace.browserImagePicker ? workspace.pickImage() : imagePicker.open() }
                                GhostButton { text: "Clear image"; enabled: workspace.imageLoading || workspace.imagePreview.length > 0; onClicked: workspace.clearImage() }
                                Caption { text: workspace.connected ? "Stays on this device unless you choose Interpret" : "On this device only"; Layout.fillWidth: true }
                            }
                            Image {
                                Layout.fillWidth: true; Layout.preferredHeight: 300
                                source: workspace.imagePreview; fillMode: Image.PreserveAspectFit
                                Accessible.name: "Selected image preview"
                            }
                            Caption { text: workspace.imageLoading ? "Loading image…" : workspace.imageNotice.length > 0 ? workspace.imageNotice : "No image selected"; Layout.fillWidth: true }
                            RowLayout {
                                visible: workspace.connected && workspace.imagePreview.length > 0
                                Layout.fillWidth: true; spacing: 10
                                Field {
                                    id: visionQuestion
                                    Layout.fillWidth: true; placeholderText: "Ask about this image (optional)"
                                    Accessible.name: "Question about the image"; maximumLength: 4000
                                    onAccepted: workspace.interpretImage(text)
                                }
                                PrimaryButton { text: "Interpret image"; enabled: !workspace.imageInterpreting; onClicked: workspace.interpretImage(visionQuestion.text) }
                            }
                            Caption { visible: workspace.connected && workspace.imagePreview.length > 0; Layout.fillWidth: true; font.pixelSize: 12; text: "Interpreting sends a reduced copy of this image to the model through your account gateway. The image is not stored; the question and answer are added to your conversation." }
                            Caption { visible: workspace.imageInterpreting; text: "Waiting for the model…"; color: window.cyan }
                            Label { visible: workspace.imageAnswer.length > 0; text: workspace.imageAnswer; textFormat: Text.PlainText; color: window.ink; wrapMode: Text.WordWrap; font.pixelSize: 14; Layout.fillWidth: true }
                        }
                    }
                    Card {
                        visible: window.page === 8; Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter
                        implicitHeight: settingsContent.implicitHeight + 48
                        ColumnLayout {
                            id: settingsContent; anchors.fill: parent; anchors.margins: 24; spacing: 10
                            SectionTitle { text: "Services"; Layout.fillWidth: true; Layout.bottomMargin: 6 }
                            Repeater {
                                model: window.serviceRows()
                                Rectangle {
                                    id: service
                                    required property var modelData
                                    readonly property color tone: window.toneColor(modelData.tone)
                                    Layout.fillWidth: true; radius: 2; color: "#020a06"; border.color: window.line
                                    implicitHeight: serviceRow.implicitHeight + 20
                                    RowLayout {
                                        id: serviceRow
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; anchors.margins: 14; spacing: 14
                                        Rectangle { implicitWidth: 8; implicitHeight: 8; radius: 4; color: service.tone }
                                        ColumnLayout {
                                            spacing: 2; Layout.fillWidth: true
                                            Label { text: service.modelData.name; color: window.ink; font.pixelSize: 14; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                                            Caption { text: service.modelData.text; Layout.fillWidth: true }
                                        }
                                        Pill { text: service.modelData.label; tint: service.modelData.tone === "off" ? window.muted : service.tone }
                                    }
                                }
                            }
                        }
                    }
                    Card {
                        visible: window.page === 8; Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter
                        implicitHeight: privacyContent.implicitHeight + 48
                        ColumnLayout {
                            id: privacyContent; anchors.fill: parent; anchors.margins: 24; spacing: 10
                            SectionTitle { text: "Privacy"; Layout.fillWidth: true; Layout.bottomMargin: 6 }
                            Repeater {
                                model: [
                                    "Your conversations, memories, skills and rules are saved in your account as plain text, and only while you are signed in.",
                                    "Messages, and the files you add in the Developer workspace, are sent to the model set on your account gateway" + (workspace.modelName.length > 0 ? " (currently " + workspace.modelName + ")" : "") + " when you ask for something.",
                                    "An image leaves this device only when you choose Interpret. It is not stored.",
                                    workspace.voice.openMicrophone ? "Always listening is on: everything heard near this device while this page is open is sent to the model as a request. Camera and location are never used." : workspace.voice.handsFree ? "Hands-free is on: the microphone stays on while this page is open, and the browser's speech service hears what is said nearby. Camera and location are never used." : "The microphone is used only while you press the microphone button, unless you turn on hands-free. Camera and location are never used.",
                                    workspace.sessionPersists ? "You stay signed in on this device until you sign out. Signing out removes the saved sign-in." : "Your sign-in lasts until you close MNAKA. It is not kept on this device."
                                ]
                                RowLayout {
                                    id: fact
                                    required property string modelData
                                    Layout.fillWidth: true; spacing: 10
                                    Label { text: ">"; color: window.cyan; Layout.alignment: Qt.AlignTop }
                                    Caption { text: fact.modelData; font.pixelSize: 14; Layout.fillWidth: true }
                                }
                            }
                            GhostButton { text: workspace.connected ? "Delete saved conversation" : "Clear this session"; Layout.topMargin: 8; onClicked: clearDialog.open() }
                        }
                    }
                    Card {
                        visible: window.page === 8; Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter
                        implicitHeight: accountContent.implicitHeight + 48
                        ColumnLayout {
                            id: accountContent; anchors.fill: parent; anchors.margins: 24; spacing: 12
                            Heading { text: "Account" }
                            Caption {
                                Layout.fillWidth: true
                                text: !workspace.accountConfigured ? "No account service is configured for this build."
                                    : window.signedIn ? "Signed in as " + workspace.accountEmail + ". " + (workspace.sessionPersists ? "You stay signed in on this device until you sign out." : "The session ends when you close MNAKA.") + " Your conversations, memories, skills and rules are saved to your account."
                                    : "Your email and password are sent to the configured Supabase project over HTTPS. After you sign in, your messages go to the model set on your account gateway and your data is saved to your account."
                            }
                            Field {
                                id: accountEmailField; visible: workspace.accountConfigured && !window.signedIn
                                Layout.fillWidth: true; Layout.maximumWidth: 420
                                placeholderText: "Email"; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                                Accessible.name: "Account email"
                            }
                            Field {
                                id: accountPasswordField; visible: accountEmailField.visible
                                Layout.fillWidth: true; Layout.maximumWidth: 420
                                placeholderText: "Password"; echoMode: TextInput.Password
                                Accessible.name: "Account password"
                                onAccepted: { workspace.signIn(accountEmailField.text, text); clear() }
                            }
                            RowLayout {
                                visible: accountEmailField.visible; spacing: 10
                                PrimaryButton { text: "Sign in"; enabled: !workspace.accountBusy; onClicked: { workspace.signIn(accountEmailField.text, accountPasswordField.text); accountPasswordField.clear() } }
                                GhostButton { text: "Create account"; enabled: !workspace.accountBusy; onClicked: { workspace.signUp(accountEmailField.text, accountPasswordField.text); accountPasswordField.clear() } }
                                GhostButton { text: "Resend confirmation email"; enabled: !workspace.accountBusy && accountEmailField.text.trim().length > 0; onClicked: workspace.resendConfirmation(accountEmailField.text) }
                            }
                            GhostButton { visible: window.signedIn; text: "Sign out"; onClicked: workspace.signOut() }
                            Caption { text: workspace.accountBusy ? "Contacting the account service…" : workspace.accountNotice; visible: text.length > 0; Layout.fillWidth: true }
                        }
                    }
                    Caption { Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter; Layout.fillWidth: true; text: "MNAKA v1.1 specification · Foundation preview · Production release gates remain pending"; font.pixelSize: 11 }
                    Item { implicitHeight: 20 }
                }
            }
        }
    }
    Dialog {
        id: clearDialog; anchors.centerIn: parent
        title: "Clear session history?"; modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label { text: workspace.connected ? "Delete your saved conversation from your account and clear this session's tasks." : "Remove temporary messages and tasks from this session."; color: window.ink; wrapMode: Text.WordWrap; width: Math.min(320, window.width - 80) }
        onAccepted: workspace.clearHistory()
    }
}
