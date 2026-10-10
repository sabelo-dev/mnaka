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
    minimumWidth: 380; minimumHeight: 600
    visible: true
    title: "MNAKA · AI workspace"
    color: "#070b18"
    property color ink: "#e8ecf8"
    property color muted: "#8b95b5"
    property color accent: "#5b61ff"
    property color cyan: "#5ad2ff"
    property color panel: "#0c1224"
    property color line: "#1a2340"
    // Code font: Qt for WebAssembly bundles DejaVu Sans Mono.
    readonly property string mono: Qt.platform.os === "windows" ? "Consolas" : "DejaVu Sans Mono"
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
        file: "M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z M14 2v6h6",
        down: "M6 9l6 6 6-6",
        ring: "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18z",
        info: "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18z M12 16v-5 M12 7.5v.5"
    })
    readonly property var pageIcons: [icons.home, icons.tasks, icons.code, icons.monitor, icons.layers, icons.folder, icons.eye, icons.zap, icons.settings]
    readonly property bool signedIn: workspace.accountEmail.length > 0
    readonly property string accountName: signedIn ? workspace.accountEmail.split("@")[0] : ""
    readonly property string greeting: { const hour = new Date().getHours(); return hour < 12 ? "Good morning" : hour < 18 ? "Good afternoon" : "Good evening" }
    // Developer workspace sample. Opt-in and labelled as sample data: it is static presentation
    // content, not read from any project.
    property bool devSample: false
    property int devTab: 0
    readonly property var sampleTree: [
        { name: "qt-sample-app", depth: 0, folder: true, changed: false },
        { name: "cmake", depth: 1, folder: true, changed: false },
        { name: "CMakeLists.txt", depth: 2, folder: false, changed: false },
        { name: "src", depth: 1, folder: true, changed: false },
        { name: "main.cpp", depth: 2, folder: false, changed: true },
        { name: "qml", depth: 2, folder: true, changed: false },
        { name: "Main.qml", depth: 3, folder: false, changed: true },
        { name: "tests", depth: 1, folder: true, changed: false },
        { name: "tst_main.cpp", depth: 2, folder: false, changed: false },
        { name: "CMakeLists.txt", depth: 2, folder: false, changed: false }
    ]
    readonly property var sampleDiff: [
        { path: "src/main.cpp", added: 3, removed: 0, hunk: "@@ -1,7 +1,10 @@", lines: [
            [1, " ", "#include <QGuiApplication>"], [2, " ", "#include <QQmlApplicationEngine>"], [3, " ", "#include <QUrl>"],
            [4, " ", "int main(int argc, char *argv[])"], [5, " ", "{"], [6, " ", "    QGuiApplication app(argc, argv);"],
            [7, "+", "    app.setApplicationName(\"Mnaka Sample\");"], [8, "+", "    app.setOrganizationName(\"Mnaka\");"],
            [9, "+", "    app.setOrganizationDomain(\"mnaka.example\");"], [10, " ", "    QQmlApplicationEngine engine;"]] },
        { path: "qml/Main.qml", added: 4, removed: 2, hunk: "@@ -12,7 +12,9 @@ ApplicationWindow {", lines: [
            [12, " ", "    width: 640"], [13, " ", "    height: 480"], [14, " ", "    visible: true"],
            [15, "-", "    title: \"Qt Quick Application\""], [16, "-", "    color: \"#1e1e1e\""],
            [15, "+", "    title: \"Mnaka Sample App\""], [16, "+", "    color: \"#101322\""],
            [17, "+", "    property string statusText: \"Ready\""], [18, "+", "    MnakaStatusBar { id: statusBar; text: statusText }"],
            [19, " ", "}"]] }
    ]
    readonly property var sampleChecklist: [
        { title: "Inspect repository", detail: "Read project structure and key files.", done: true },
        { title: "Prepare patch", detail: "Draft the proposed changes.", done: true },
        { title: "Run tests", detail: "Build and test in an isolated sandbox.", done: true },
        { title: "Await approval", detail: "Review the changes and apply when ready.", done: false }
    ]
    function lastReply() {
        const list = workspace.messages
        return list.length > 0 && list[list.length - 1].role === "assistant" ? list[list.length - 1].text : ""
    }
    function capabilityFor(id) {
        const list = workspace.capabilities
        for (let i = 0; i < list.length; ++i) if (list[i].id === id) return list[i]
        return { enabled: false, status: "Service unavailable" }
    }
    function activeTaskCount() {
        let count = 0
        const list = workspace.tasks
        for (let i = 0; i < list.length; ++i) if (list[i].active) ++count
        return count
    }
    WorkspaceModel { id: workspace }
    FileDialog {
        id: imagePicker
        title: "Select an image"
        nameFilters: ["Images (*.png *.jpg *.jpeg)"]
        onAccepted: workspace.selectImage(selectedFile)
    }
    palette.window: "#070b18"
    palette.base: "#101831"
    palette.text: window.ink
    palette.windowText: window.ink
    palette.button: "#18213d"
    palette.buttonText: window.ink
    palette.highlight: window.accent
    palette.highlightedText: "#ffffff"
    palette.placeholderText: window.muted

    component Caption: Label {
        color: window.muted; font.pixelSize: 13; wrapMode: Text.WordWrap
        textFormat: Text.PlainText
    }
    component Heading: Label {
        color: window.ink; font.pixelSize: 21; font.weight: Font.DemiBold
        wrapMode: Text.WordWrap; textFormat: Text.PlainText
    }
    component Card: Rectangle {
        radius: 14; color: window.panel; border.color: window.line
    }
    component Pill: Rectangle {
        id: pill
        property string text
        property color tint: window.cyan
        implicitWidth: pillLabel.implicitWidth + 22; implicitHeight: 26
        radius: 13; color: Qt.rgba(tint.r, tint.g, tint.b, 0.14)
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
        implicitWidth: 36; implicitHeight: 36; radius: width / 2
        color: "#1a2342"; border.color: "#2a3560"
        Label { visible: !avatar.logo; anchors.centerIn: parent; text: avatar.letter; color: window.ink; font.pixelSize: 14; font.weight: Font.DemiBold }
        Image { visible: avatar.logo; anchors.fill: parent; anchors.margins: 6; source: "assets/logo.png"; sourceSize: Qt.size(64, 64); fillMode: Image.PreserveAspectFit }
    }
    component PrimaryButton: Button {
        id: primary
        implicitHeight: 38; leftPadding: 18; rightPadding: 18
        contentItem: Label { text: primary.text; color: "#ffffff"; font.weight: Font.DemiBold; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 9; color: !primary.enabled ? "#262d52" : primary.down ? "#4a50e0" : primary.hovered ? "#6b71ff" : window.accent; border.color: primary.activeFocus ? window.cyan : "transparent" }
    }
    component GhostButton: Button {
        id: ghost
        implicitHeight: 38; leftPadding: 16; rightPadding: 16
        contentItem: Label { text: ghost.text; color: ghost.enabled ? window.ink : window.muted; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 9; color: ghost.hovered ? "#172040" : "transparent"; border.color: ghost.activeFocus ? window.cyan : "#2a3560" }
    }
    component Field: TextField {
        id: field
        color: window.ink; placeholderTextColor: window.muted; font.pixelSize: 14
        leftPadding: 12; rightPadding: 12; implicitHeight: 40
        background: Rectangle { radius: 9; color: "#0a1022"; border.color: field.activeFocus ? window.accent : "#2a3560" }
    }
    component SectionTitle: RowLayout {
        id: section
        property string icon
        property string text
        property int count: -1
        spacing: 10
        Icon { visible: section.icon.length > 0; path: section.icon.length > 0 ? section.icon : window.icons.check; tint: window.cyan }
        Label { text: section.text; color: window.ink; font.pixelSize: 16; font.weight: Font.DemiBold }
        Rectangle {
            visible: section.count >= 0; implicitWidth: 24; implicitHeight: 22; radius: 11; color: "#1a2342"
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
                    GradientStop { position: 0.0; color: "#663d7dff" }
                    GradientStop { position: 0.62; color: "#2a3d5dff" }
                    GradientStop { position: 1.0; color: "#003d5dff" }
                }
                PathAngleArc { centerX: orb.size / 2; centerY: orb.size / 2; radiusX: orb.size / 2; radiusY: orb.size / 2; startAngle: 0; sweepAngle: 360 }
            }
            ShapePath {
                strokeColor: "#5587d4ff"; strokeWidth: 1
                fillGradient: RadialGradient {
                    centerX: orb.size * 0.5; centerY: orb.size * 0.5; focalX: orb.size * 0.38; focalY: orb.size * 0.34; centerRadius: orb.size * 0.36
                    GradientStop { position: 0.0; color: "#b4f1ff" }
                    GradientStop { position: 0.3; color: "#3a9bff" }
                    GradientStop { position: 0.68; color: "#5a3df0" }
                    GradientStop { position: 1.0; color: "#1c1766" }
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
                        strokeColor: swirl.modelData === 60 ? "#66d9c8ff" : "#55ffffff"; strokeWidth: 1.4; fillColor: "transparent"
                        PathAngleArc { centerX: swirl.width / 2; centerY: swirl.height / 2; radiusX: swirl.width / 2 - 1; radiusY: swirl.width * 0.2; startAngle: 0; sweepAngle: 360 }
                    }
                }
            }
        }
    }
    component SamplePill: Pill { text: "SAMPLE DATA"; tint: "#f5b544" }
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
        property string detail
        property string engineId
        property int destination
        readonly property var capability: window.capabilityFor(engineId)
        implicitHeight: engineColumn.implicitHeight + 28
        color: engineButton.hovered ? "#101a33" : window.panel
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
            Caption { text: engine.detail; font.pixelSize: 12; Layout.fillWidth: true }
            RowLayout {
                spacing: 7; Layout.topMargin: 2; Layout.fillWidth: true
                Rectangle { implicitWidth: 8; implicitHeight: 8; radius: 4; color: engine.capability.enabled ? "#22c55e" : "#56608a" }
                Caption { text: engine.capability.enabled ? "Ready" : engine.capability.status; font.pixelSize: 12; color: engine.capability.enabled ? "#4ade80" : window.muted; maximumLineCount: 2; elide: Text.ElideRight; Layout.fillWidth: true }
            }
        }
        Button {
            id: engineButton; anchors.fill: parent; flat: true; text: ""
            Accessible.name: "Open " + engine.name
            background: Rectangle { color: "transparent"; radius: 14; border.color: engineButton.activeFocus ? window.cyan : "transparent" }
            onClicked: window.page = engine.destination
        }
    }
    Shortcut { sequence: "Ctrl+L"; onActivated: { window.page = 0; composer.forceActiveFocus() } }
    Shortcut { sequence: "Escape"; onActivated: workspace.cancel() }

    RowLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            Layout.preferredWidth: 224; Layout.fillHeight: true
            visible: !window.compact; color: "#090e1d"
            Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: window.line }
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 14; spacing: 4
                RowLayout {
                    spacing: 10; Layout.topMargin: 8; Layout.leftMargin: 8; Layout.bottomMargin: 22
                    Image { source: "assets/logo.png"; sourceSize: Qt.size(72, 72); Layout.preferredWidth: 34; Layout.preferredHeight: 34; fillMode: Image.PreserveAspectFit; Accessible.name: "MNAKA logo" }
                    Label { text: "MNAKA"; color: window.ink; font.pixelSize: 20; font.weight: Font.Bold; font.letterSpacing: 1 }
                }
                Repeater {
                    model: window.pages
                    Button {
                        id: navButton
                        required property int index
                        required property string modelData
                        Layout.fillWidth: true; implicitHeight: 46
                        text: modelData; checkable: true; checked: window.page === index
                        Accessible.name: modelData
                        onClicked: window.page = index
                        contentItem: RowLayout {
                            spacing: 14
                            Icon { path: window.pageIcons[navButton.index]; tint: navButton.checked ? "#ffffff" : window.muted; Layout.leftMargin: 12 }
                            Label { text: navButton.text; color: navButton.checked ? "#ffffff" : window.muted; font.pixelSize: 14; font.weight: navButton.checked ? Font.DemiBold : Font.Normal; Layout.fillWidth: true }
                        }
                        background: Rectangle {
                            radius: 10; border.color: navButton.activeFocus ? window.cyan : "transparent"
                            color: navButton.checked ? "transparent" : navButton.hovered ? "#111a33" : "transparent"
                            gradient: navButton.checked ? activeNav : null
                            Gradient { id: activeNav; orientation: Gradient.Horizontal; GradientStop { position: 0; color: "#2a3190" } GradientStop { position: 1; color: "#1a2158" } }
                        }
                    }
                }
                Item { Layout.fillHeight: true }
                Button {
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
                            Caption { text: workspace.connected ? "Connected" : workspace.mockSession ? "Mock session" : "Signed out"; font.pixelSize: 11; elide: Text.ElideRight; wrapMode: Text.NoWrap; Layout.fillWidth: true }
                        }
                        Icon { path: window.icons.chevron; size: 14 }
                    }
                    background: Rectangle { radius: 10; color: accountChip.hovered ? "#111a33" : "transparent"; border.color: accountChip.activeFocus ? window.cyan : "transparent" }
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 84; color: "#090e1d"
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: window.line }
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: window.compact ? 14 : 22; anchors.rightMargin: window.compact ? 14 : 22; spacing: 14
                    ComboBox { visible: window.compact; model: window.pages; currentIndex: window.page; onActivated: window.page = currentIndex; Accessible.name: "Workspace screen"; Layout.preferredWidth: 150 }
                    ColumnLayout {
                        visible: !window.compact; spacing: 2; Layout.fillWidth: true
                        Label { text: window.titles[window.page]; color: window.ink; font.pixelSize: 26; font.weight: Font.Bold; elide: Text.ElideRight; Layout.fillWidth: true }
                        Caption { text: window.page === 2 ? (window.devSample ? "Sample Qt project · Illustration" : "No project open") : window.descriptions[window.page]; font.pixelSize: 14; elide: Text.ElideRight; wrapMode: Text.NoWrap; Layout.fillWidth: true }
                    }
                    Item { visible: window.compact; Layout.fillWidth: true }
                    RowLayout {
                        visible: !window.compact; spacing: 8
                        Rectangle { implicitWidth: 8; implicitHeight: 8; radius: 4; color: workspace.connected ? "#22c55e" : workspace.mockSession ? "#f5b544" : "#56608a" }
                        Caption { text: workspace.connected ? "Connected" : workspace.mockSession ? "Mock session" : "Not connected" }
                    }
                    Pill { text: "●  " + workspace.state; tint: workspace.busy ? "#b79cff" : window.cyan }
                    ComboBox { id: mode; model: ["Assistant", "Developer", "Autonomous"]; Layout.preferredWidth: 140; Accessible.name: "Workflow mode" }
                }
            }
            ScrollView {
                id: scroll
                Layout.fillWidth: true; Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    id: content
                    readonly property int gutter: window.compact ? 14 : 22
                    width: scroll.availableWidth
                    spacing: 16
                    Item { implicitHeight: 2 }
                    Card {
                        Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter
                        implicitHeight: statusText.implicitHeight + 24
                        color: "#15132e"; border.color: "#2d2760"
                        Caption { id: statusText; anchors.fill: parent; anchors.margins: 12; color: "#c3b8ee"; text: workspace.connected ? "Connected. Messages go to your own model through your account gateway. Conversations, memories, skills and rules are saved to your account as plain text." : workspace.mockSession ? "MOCK SESSION. Replies come from a deterministic fixture, not a reasoning model. Tasks run through the broker's fixture tools only: no files, applications or networks are touched, and nothing is saved when you close MNAKA." : (workspace.accountConfigured ? "Signed out. Sign in under Settings to use the assistant. Nothing is sent or saved while you are signed out." : "No account service is configured for this build, so the assistant is unavailable.") }
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
                                        spacing: 2; Layout.fillWidth: true
                                        Heading { text: window.greeting + (window.signedIn ? ", " + window.accountName : ""); Layout.fillWidth: true }
                                        Caption { text: "How can I help you today?"; font.pixelSize: 14 }
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
                                            Layout.fillWidth: true; radius: 12
                                            color: message.mine ? "#1c2468" : "#101831"; border.color: message.mine ? "#2f3a94" : window.line
                                            implicitHeight: bubble.implicitHeight + 24
                                            ColumnLayout {
                                                id: bubble
                                                anchors.fill: parent; anchors.margins: 12; spacing: 4
                                                Label { text: message.mine ? "YOU" : message.modelData.mock ? "MNAKA · MOCK RESPONSE" : "MNAKA"; color: message.mine ? window.cyan : "#b79cff"; font.pixelSize: 10; font.weight: Font.Bold }
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
                                Flow {
                                    Layout.fillWidth: true; spacing: 8
                                    GhostButton { text: "Traffic on the N1"; implicitHeight: 32; onClicked: { composer.text = "Mnaka, tell me about traffic on the N1"; composer.forceActiveFocus() } }
                                    GhostButton { text: "Inspect a Qt project"; implicitHeight: 32; onClicked: { composer.text = "Inspect this Qt project and propose a fix"; composer.forceActiveFocus() } }
                                    GhostButton { visible: workspace.mockSession; text: "Approval demo"; implicitHeight: 32; onClicked: { composer.text = "Mnaka, run the review demo"; composer.forceActiveFocus() } }
                                }
                                Rectangle {
                                    Layout.fillWidth: true; implicitHeight: Math.min(160, Math.max(56, composer.implicitHeight + 16))
                                    radius: 14; color: "#0a1022"; border.color: composer.activeFocus ? window.accent : "#2a3560"
                                    RowLayout {
                                        anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 8; spacing: 6
                                        ScrollView {
                                            Layout.fillWidth: true; Layout.fillHeight: true
                                            TextArea {
                                                id: composer
                                                placeholderText: "Ask Mnaka…"; placeholderTextColor: window.muted
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
                                            id: send
                                            implicitWidth: 40; implicitHeight: 40
                                            enabled: workspace.busy || composer.text.trim().length > 0
                                            Accessible.name: workspace.busy ? "Cancel task" : "Send message"
                                            onClicked: { if (workspace.busy) workspace.cancel(); else if (workspace.submit(composer.text, mode.currentText)) composer.text = "" }
                                            contentItem: Item {
                                                Icon { visible: !workspace.busy; anchors.centerIn: parent; path: window.icons.send; tint: "#ffffff"; size: 18 }
                                                Rectangle { visible: workspace.busy; anchors.centerIn: parent; width: 12; height: 12; radius: 2; color: "#ffffff" }
                                            }
                                            background: Rectangle { radius: 20; color: !send.enabled ? "#262d52" : send.hovered ? "#6b71ff" : window.accent; border.color: send.activeFocus ? window.cyan : "transparent" }
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
                                gradient: Gradient { GradientStop { position: 0; color: "#0a1024" } GradientStop { position: 1; color: "#0d1430" } }
                                ColumnLayout {
                                    id: orbColumn
                                    anchors.centerIn: parent; width: parent.width - 40; spacing: 6
                                    Orb { Layout.alignment: Qt.AlignHCenter; active: workspace.busy; size: window.compact ? 170 : 230 }
                                    Label { Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 6; text: workspace.busy ? workspace.state : workspace.connected ? "Ready" : workspace.mockSession ? "Mock session ready" : "Not connected"; color: window.ink; font.pixelSize: 19; font.weight: Font.DemiBold }
                                    Caption { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: workspace.state === "Awaiting approval" ? "Review the requested action under Approval required." : workspace.busy ? "Working on your request." : workspace.connected ? "Type a message to talk to your model. Voice input is unavailable." : workspace.mockSession ? "Type a message to run a fixture task. Voice input is unavailable." : "Sign in under Settings to connect. Voice input is unavailable." }
                                }
                            }
                            GridLayout {
                                columns: window.compact ? 1 : window.width >= 1500 ? 3 : 2; columnSpacing: 12; rowSpacing: 12; Layout.fillWidth: true; Layout.fillHeight: false
                                uniformCellWidths: true
                                EngineCard { Layout.fillWidth: true; Layout.fillHeight: true; icon: window.icons.code; name: "Software Engineering"; detail: "Scoped patches, isolated builds and review evidence."; engineId: "software"; destination: 2 }
                                EngineCard { Layout.fillWidth: true; Layout.fillHeight: true; icon: window.icons.monitor; name: "Computer Control"; detail: "Registered adapters and foreground verification."; engineId: "control"; destination: 3 }
                                EngineCard { Layout.fillWidth: true; Layout.fillHeight: true; icon: window.icons.layers; name: "Self-extending Skills"; detail: "Validated manifests, approval and rollback."; engineId: "skills"; destination: 4 }
                                EngineCard { Layout.fillWidth: true; Layout.fillHeight: true; icon: window.icons.chart; name: "Project Intelligence"; detail: "Sourced memories, decisions and deletion."; engineId: "memory"; destination: 5 }
                                EngineCard { Layout.fillWidth: true; Layout.fillHeight: true; icon: window.icons.eye; name: "Vision & Spatial"; detail: "Selected images and a bounded scene viewer."; engineId: "vision"; destination: 6 }
                                EngineCard { Layout.fillWidth: true; Layout.fillHeight: true; icon: window.icons.zap; name: "Proactive Intelligence"; detail: "Deterministic rules with explicit authorization."; engineId: "proactive"; destination: 7 }
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
                                            Layout.fillWidth: true; radius: 10; color: "#0a1022"; border.color: window.line
                                            implicitHeight: recentColumn.implicitHeight + 24
                                            ColumnLayout {
                                                id: recentColumn
                                                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 12; spacing: 6
                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    Label { text: recent.modelData.title; textFormat: Text.PlainText; color: window.ink; font.pixelSize: 13; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                                                    Pill { text: recent.modelData.state; tint: recent.modelData.state === "succeeded" ? "#4ade80" : recent.modelData.active ? "#b79cff" : window.muted }
                                                }
                                                Caption { text: recent.modelData.evidence; font.pixelSize: 12; Layout.fillWidth: true }
                                            }
                                        }
                                    }
                                }
                            }
                            Card {
                                Layout.fillWidth: true; implicitHeight: approvalBrief.implicitHeight + 36
                                border.color: workspace.approval.id !== undefined ? "#6a5bd0" : window.line
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
                                            Rectangle { implicitWidth: 3; implicitHeight: 34; radius: 2; color: upcomingRule.modelData.enabled ? window.accent : "#2a3560" }
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
                            border.color: "#6a5bd0"
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
                        visible: window.page === 2
                        Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter
                        columns: window.wide ? 3 : window.compact ? 1 : 2
                        columnSpacing: 16; rowSpacing: 16
                        Card {
                            Layout.fillWidth: true; Layout.fillHeight: !window.compact; Layout.preferredWidth: 240; Layout.minimumWidth: 0; Layout.alignment: Qt.AlignTop
                            implicitHeight: treeColumn.implicitHeight + 32
                            ColumnLayout {
                                id: treeColumn
                                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16; spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true; Layout.bottomMargin: 6
                                    SectionTitle { text: "Project"; Layout.fillWidth: true }
                                    SamplePill { visible: window.devSample }
                                    GhostButton { text: window.devSample ? "Hide sample" : "Show sample"; implicitHeight: 30; onClicked: window.devSample = !window.devSample }
                                }
                                Caption { visible: !window.devSample; Layout.fillWidth: true; text: "No project open. Opening a project needs the project broker and PostgreSQL, which are not connected." }
                                Repeater {
                                    model: window.devSample ? window.sampleTree : []
                                    Rectangle {
                                        id: node
                                        required property var modelData
                                        Layout.fillWidth: true; implicitHeight: 30; radius: 6
                                        color: node.modelData.changed ? "#131c3a" : "transparent"
                                        RowLayout {
                                            anchors.fill: parent; anchors.leftMargin: 6 + node.modelData.depth * 18; anchors.rightMargin: 8; spacing: 8
                                            Icon { path: node.modelData.folder ? window.icons.folder : window.icons.file; size: 16; tint: node.modelData.folder ? window.muted : window.cyan }
                                            Label { text: node.modelData.name; color: node.modelData.changed ? window.ink : window.muted; font.pixelSize: 13; elide: Text.ElideRight; Layout.fillWidth: true }
                                            Rectangle { visible: node.modelData.changed; implicitWidth: 7; implicitHeight: 7; radius: 4; color: "#f5b544"; Accessible.name: "Changed" }
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
                                            id: tab
                                            required property int index
                                            required property string modelData
                                            text: modelData; implicitHeight: 44; leftPadding: 22; rightPadding: 22
                                            checkable: true; checked: window.devTab === index
                                            onClicked: window.devTab = index
                                            contentItem: Label { text: tab.text; color: tab.checked ? "#8f94ff" : window.muted; font.pixelSize: 13; font.weight: tab.checked ? Font.DemiBold : Font.Normal; verticalAlignment: Text.AlignVCenter }
                                            background: Item {
                                                Rectangle { anchors.fill: parent; color: tab.hovered ? "#101a33" : "transparent"; border.color: tab.activeFocus ? window.cyan : "transparent" }
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
                                        SamplePill { visible: window.devSample }
                                        Caption { visible: window.devSample; text: window.sampleDiff.length + " files changed" }
                                    }
                                    Caption { visible: !window.devSample; Layout.fillWidth: true; text: "No proposed changes. Project picker, indexing, diff, build logs and rollback require the project broker and PostgreSQL. Builds additionally require a tested OS sandbox. Native Windows execution remains disabled." }
                                    Caption { visible: window.devSample; Layout.fillWidth: true; color: "#f5d28a"; text: "This is an illustration of how a proposed change will be reviewed. It was not read from, and will not be written to, any file on this device." }
                                    Repeater {
                                        model: window.devSample ? window.sampleDiff : []
                                        Rectangle {
                                            id: fileDiff
                                            required property var modelData
                                            property bool expanded: true
                                            Layout.fillWidth: true; radius: 10; color: "#0a1022"; border.color: window.line; clip: true
                                            implicitHeight: fileColumn.implicitHeight + 2
                                            ColumnLayout {
                                                id: fileColumn
                                                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 1; spacing: 0
                                                Button {
                                                    id: fileHeader
                                                    Layout.fillWidth: true; implicitHeight: 40
                                                    Accessible.name: (fileDiff.expanded ? "Collapse " : "Expand ") + fileDiff.modelData.path
                                                    onClicked: fileDiff.expanded = !fileDiff.expanded
                                                    contentItem: RowLayout {
                                                        spacing: 10
                                                        Icon { path: fileDiff.expanded ? window.icons.down : window.icons.chevron; size: 16; Layout.leftMargin: 10 }
                                                        Icon { path: window.icons.file; size: 16; tint: window.cyan }
                                                        Label { text: fileDiff.modelData.path; color: window.ink; font.family: window.mono; font.pixelSize: 13; elide: Text.ElideMiddle; Layout.fillWidth: true }
                                                        Label { text: "+" + fileDiff.modelData.added; color: "#4ade80"; font.family: window.mono; font.pixelSize: 13 }
                                                        Label { text: "\u2212" + fileDiff.modelData.removed; color: "#f87171"; font.family: window.mono; font.pixelSize: 13; Layout.rightMargin: 14 }
                                                    }
                                                    background: Rectangle { color: fileHeader.hovered ? "#111a33" : "transparent"; border.color: fileHeader.activeFocus ? window.cyan : "transparent" }
                                                }
                                                Rectangle {
                                                    visible: fileDiff.expanded; Layout.fillWidth: true; implicitHeight: 24; color: "#111a36"
                                                    Label { anchors.verticalCenter: parent.verticalCenter; x: 70; text: fileDiff.modelData.hunk; color: "#7f9cf5"; font.family: window.mono; font.pixelSize: 12 }
                                                }
                                                Repeater {
                                                    model: fileDiff.expanded ? fileDiff.modelData.lines : []
                                                    DiffLine {}
                                                }
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
                                    RowLayout {
                                        Layout.fillWidth: true
                                        SectionTitle { text: "Task checklist"; Layout.fillWidth: true }
                                        SamplePill { visible: window.devSample }
                                    }
                                    Caption { visible: !window.devSample; Layout.fillWidth: true; text: "No developer task is running. Steps will appear here as a task inspects a project, prepares a patch and waits for your approval." }
                                    Repeater {
                                        model: window.devSample ? window.sampleChecklist : []
                                        RowLayout {
                                            id: step
                                            required property var modelData
                                            Layout.fillWidth: true; spacing: 12
                                            Rectangle {
                                                Layout.alignment: Qt.AlignTop
                                                implicitWidth: 26; implicitHeight: 26; radius: 13
                                                color: step.modelData.done ? "#22c55e" : "transparent"; border.color: step.modelData.done ? "#22c55e" : "#7c6cf0"; border.width: 2
                                                Icon { visible: step.modelData.done; anchors.centerIn: parent; path: window.icons.check; size: 15; weight: 2.4; tint: "#06240f" }
                                            }
                                            ColumnLayout {
                                                spacing: 2; Layout.fillWidth: true
                                                Label { text: step.modelData.title; color: window.ink; font.pixelSize: 14; font.weight: Font.DemiBold }
                                                Caption { text: step.modelData.detail; Layout.fillWidth: true }
                                            }
                                        }
                                    }
                                }
                            }
                            Card {
                                visible: window.devSample
                                Layout.fillWidth: true; implicitHeight: review.implicitHeight + 36
                                ColumnLayout {
                                    id: review
                                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 18; spacing: 12
                                    SectionTitle { text: "Review before applying"; Layout.fillWidth: true }
                                    Caption { Layout.fillWidth: true; font.pixelSize: 14; text: "These changes add application metadata and a status bar to the main window." }
                                    PrimaryButton { text: "Apply changes"; enabled: false; Layout.fillWidth: true; implicitHeight: 44 }
                                    Caption { Layout.fillWidth: true; font.pixelSize: 12; text: "Applying is unavailable: there is no project to change, and no project broker or sandbox is connected." }
                                }
                            }
                            Card {
                                visible: window.devSample
                                Layout.fillWidth: true; implicitHeight: summary.implicitHeight + 36
                                ColumnLayout {
                                    id: summary
                                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 18; spacing: 12
                                    SectionTitle { text: "Summary"; Layout.fillWidth: true }
                                    RowLayout {
                                        Layout.fillWidth: true; spacing: 10
                                        Icon { path: window.icons.file; size: 18 }
                                        Label { text: window.sampleDiff.length + " files changed"; color: window.ink; font.pixelSize: 14; Layout.fillWidth: true }
                                        Label { text: "+" + (window.sampleDiff[0].added + window.sampleDiff[1].added); color: "#4ade80"; font.family: window.mono; font.pixelSize: 14 }
                                        Label { text: "\u2212" + (window.sampleDiff[0].removed + window.sampleDiff[1].removed); color: "#f87171"; font.family: window.mono; font.pixelSize: 14 }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true; spacing: 10
                                        Icon { path: window.icons.info; size: 18; tint: "#f5b544"; Layout.alignment: Qt.AlignTop }
                                        Caption { text: "Sample data. No build or test has run."; Layout.fillWidth: true }
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
                            Caption {
                                readonly property string reply: window.lastReply()
                                visible: text.length > 0; Layout.fillWidth: true
                                color: workspace.streamingText.length > 0 ? window.cyan : window.muted
                                text: workspace.streamingText.length > 0 ? workspace.streamingText : workspace.notice.length > 0 ? workspace.notice : reply
                            }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 10
                                Field {
                                    id: devPrompt
                                    Layout.fillWidth: true; implicitHeight: 44
                                    placeholderText: "Ask Mnaka about this project…"
                                    Accessible.name: "Developer request to MNAKA"
                                    onAccepted: { if (workspace.submit(text, "Developer")) clear() }
                                }
                                Button {
                                    id: devSend
                                    implicitWidth: 44; implicitHeight: 44
                                    enabled: workspace.busy || devPrompt.text.trim().length > 0
                                    Accessible.name: workspace.busy ? "Cancel task" : "Send developer request"
                                    onClicked: { if (workspace.busy) workspace.cancel(); else if (workspace.submit(devPrompt.text, "Developer")) devPrompt.clear() }
                                    contentItem: Item {
                                        Icon { visible: !workspace.busy; anchors.centerIn: parent; path: window.icons.send; tint: "#ffffff"; size: 18 }
                                        Rectangle { visible: workspace.busy; anchors.centerIn: parent; width: 12; height: 12; radius: 2; color: "#ffffff" }
                                    }
                                    background: Rectangle { radius: 22; color: !devSend.enabled ? "#262d52" : devSend.hovered ? "#6b71ff" : window.accent; border.color: devSend.activeFocus ? window.cyan : "transparent" }
                                }
                            }
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
                            Caption { Layout.fillWidth: true; text: "Facts you save here are sent to your model with every message, and stored in your account as plain text. Projects and indexing need the desktop app." }
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
                                    Layout.fillWidth: true; radius: 10; color: "#0a1022"; border.color: window.line
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
                                    Layout.fillWidth: true; radius: 10; color: "#0a1022"; border.color: window.line
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
                                radius: 10; color: "#0a1022"; border.color: skillEditor.activeFocus ? window.accent : "#2a3560"
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
                                    Layout.fillWidth: true; radius: 10; color: "#0a1022"; border.color: window.line
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
                            Caption { visible: workspace.connected && workspace.imagePreview.length > 0; Layout.fillWidth: true; font.pixelSize: 12; text: "Interpreting sends a reduced copy of this image to your model through your account gateway. The image is not stored; the question and answer are added to your conversation." }
                            Caption { visible: workspace.imageInterpreting; text: "Waiting for your model…"; color: window.cyan }
                            Label { visible: workspace.imageAnswer.length > 0; text: workspace.imageAnswer; textFormat: Text.PlainText; color: window.ink; wrapMode: Text.WordWrap; font.pixelSize: 14; Layout.fillWidth: true }
                        }
                    }
                    Card {
                        visible: window.page === 8; Layout.fillWidth: true; Layout.leftMargin: content.gutter; Layout.rightMargin: content.gutter
                        implicitHeight: settingsContent.implicitHeight + 48
                        ColumnLayout {
                            id: settingsContent; anchors.fill: parent; anchors.margins: 24; spacing: 16
                            Heading { text: "Private by default" }
                            Caption { text: "Camera, audio and location capture are unavailable. Selected images stay on this device unless you choose to interpret one. Preferences are session-only. Provider credentials and database connection details are never exposed to QML."; Layout.fillWidth: true }
                            Label { text: workspace.connected ? "Model provider: your own model, through the account gateway" : workspace.mockSession ? "Model provider: MockModelProvider (deterministic fixture)" : "Model provider: not connected"; color: window.cyan; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            Caption { text: "Cloud transmission: " + (workspace.connected ? "messages, saved data and images you choose to interpret" : workspace.accountConfigured ? "account sign-in only" : "disabled") + "\nMicrophone / wake detection: unavailable\nCamera / gesture detection: unavailable\nPostgreSQL: " + (Qt.platform.os === "wasm" ? "not part of the browser build" : "desktop adapter built; UI persistence not connected") + "\nBroker: " + (workspace.connected ? "conversation steps only; no OS access" : workspace.mockSession ? "fixture tools only (demo.echo, demo.review); no OS access" : "execution disabled") + "\nRelay and mobile pairing: unavailable\nModes affect workflows, never permissions."; Layout.fillWidth: true }
                            Label { text: "ENGINE REGISTRY"; color: window.muted; font.pixelSize: 11; font.letterSpacing: 2 }
                            Repeater {
                                model: workspace.capabilities
                                Caption {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    text: modelData.id + " · " + (modelData.enabled ? "enabled" : "disabled") + " · " + modelData.status
                                }
                            }
                            GhostButton { text: "Delete session history"; onClicked: clearDialog.open() }
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
                                    : window.signedIn ? "Signed in as " + workspace.accountEmail + ". The session is kept in memory only and ends when you close MNAKA. Your conversations, memories, skills and rules are saved to your account."
                                    : "Your email and password are sent to the configured Supabase project over HTTPS. After you sign in, your messages go to your own model and your data is saved to your account."
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
