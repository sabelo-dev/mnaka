pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import Mnaka.App

ApplicationWindow {
    id: window
    width: 1240; height: 820
    minimumWidth: 380; minimumHeight: 600
    visible: true
    title: "MNAKA · AI workspace"
    color: "#0c101b"
    property color ink: "#e4edf8"
    property color muted: "#91a1b8"
    property color cyan: "#64e4d1"
    property int page: 0
    property bool compact: width < 850
    property var pages: ["Home", "Task centre", "Developer", "Control", "Skills", "Projects", "Vision", "Proactive", "Settings"]
    property var descriptions: ["Your conversation, connected to your workspace.", "Review progress, evidence and cancellations.", "From an authorized project to a reviewable change.", "Defined actions. Verified destinations.", "Declarative capabilities, with bounded permissions.", "Sourced facts and decisions, under your control.", "Interpret selected images and explore spatial scenes.", "User-enabled rules with explicit scope.", "Privacy, providers and device preferences."]
    WorkspaceModel { id: workspace }
    FileDialog {
        id: imagePicker
        title: "Select an image"
        nameFilters: ["Images (*.png *.jpg *.jpeg)"]
        onAccepted: workspace.selectImage(selectedFile)
    }
    palette.window: "#0c101b"
    palette.base: "#171e2e"
    palette.text: window.ink
    palette.windowText: window.ink
    palette.button: "#232d42"
    palette.buttonText: window.ink
    palette.highlight: "#397e82"

    component Caption: Label {
        color: window.muted; font.pixelSize: 13; wrapMode: Text.WordWrap
        textFormat: Text.PlainText
    }
    component Heading: Label {
        color: window.ink; font.pixelSize: 23; font.weight: Font.DemiBold
        wrapMode: Text.WordWrap; textFormat: Text.PlainText
    }
    component Card: Rectangle {
        radius: 16; color: "#141c2b"; border.color: "#263148"
    }
    component Pill: Rectangle {
        property string text
        implicitWidth: pillLabel.implicitWidth + 22; implicitHeight: 28
        radius: 14; color: "#163638"
        Label { id: pillLabel; anchors.centerIn: parent; text: parent.text; color: window.cyan; font.pixelSize: 11 }
    }
    component Capability: Card {
        id: capability
        property string number
        property string name
        property string detail
        property int destination
        implicitHeight: 142
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 18; spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Label { text: capability.number; color: window.cyan; font.pixelSize: 12 }
                Item { Layout.fillWidth: true }
                Label { text: "PLANNED"; color: window.muted; font.pixelSize: 10 }
            }
            Label { text: capability.name; color: window.ink; font.pixelSize: 16; font.weight: Font.DemiBold }
            Caption { text: capability.detail; Layout.fillWidth: true }
        }
        Button {
            id: capabilityButton; anchors.fill: parent; flat: true; text: ""
            Accessible.name: "Open " + capability.name
            background: Rectangle { color: capabilityButton.hovered ? "#1064e4d1" : "transparent"; radius: 16; border.color: capabilityButton.activeFocus ? window.cyan : "transparent" }
            onClicked: window.page = capability.destination
        }
    }
    Shortcut { sequence: "Ctrl+L"; onActivated: { window.page = 0; composer.forceActiveFocus() } }
    Shortcut { sequence: "Escape"; onActivated: workspace.cancel() }

    RowLayout {
        anchors.fill: parent; spacing: 0
        Rectangle {
            Layout.preferredWidth: 216; Layout.fillHeight: true
            visible: !window.compact; color: "#101623"; border.color: "#202a3d"
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 20; spacing: 8
                Label { text: "M N A K A"; color: window.cyan; font.pixelSize: 23; font.weight: Font.Bold; Layout.topMargin: 12 }
                Caption { text: "LOCAL-FIRST WORKSPACE"; font.pixelSize: 10; Layout.bottomMargin: 28 }
                Repeater {
                    model: window.pages
                    Button {
                        id: navButton
                        required property int index
                        required property string modelData
                        Layout.fillWidth: true; implicitHeight: 43
                        text: modelData; checkable: true; checked: window.page === index
                        Accessible.name: modelData
                        onClicked: window.page = index
                        contentItem: Label { text: navButton.text; color: navButton.checked ? window.cyan : window.muted; verticalAlignment: Text.AlignVCenter; leftPadding: 12 }
                        background: Rectangle { radius: 8; color: navButton.checked ? "#20373f" : navButton.hovered ? "#1a2435" : "transparent"; border.color: navButton.activeFocus ? window.cyan : "transparent" }
                    }
                }
                Item { Layout.fillHeight: true }
                Pill { text: "MNAKA" }
                Caption { text: workspace.mockSession ? "v0.1 · Mock session" : "v0.1 · Services unavailable"; Layout.topMargin: 8 }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 74; color: "#101623"
                RowLayout {
                    anchors.fill: parent; anchors.margins: window.compact ? 14 : 28
                    ComboBox { visible: window.compact; model: window.pages; currentIndex: window.page; onActivated: window.page = currentIndex; Accessible.name: "Workspace screen"; Layout.preferredWidth: 145 }
                    Label { visible: !window.compact; text: "Workspace / " + window.pages[window.page]; color: window.muted; font.pixelSize: 13 }
                    Item { Layout.fillWidth: true }
                    Pill { text: "●  " + workspace.state }
                    ComboBox { id: mode; model: ["Assistant", "Developer", "Autonomous"]; Layout.preferredWidth: window.compact ? 120 : 150; Accessible.name: "Workflow mode" }
                }
            }
            ScrollView {
                id: scroll
                Layout.fillWidth: true; Layout.fillHeight: true
                contentWidth: availableWidth
                clip: true
                ColumnLayout {
                    width: scroll.availableWidth
                    spacing: 20
                    Item { implicitHeight: 4 }
                    ColumnLayout {
                        Layout.fillWidth: true; Layout.leftMargin: 28; Layout.rightMargin: 28; spacing: 8
                        Heading { text: window.page === 0 ? "What can we work on today?" : window.pages[window.page]; Layout.fillWidth: true; font.pixelSize: window.compact ? 26 : 32 }
                        Caption { text: window.descriptions[window.page]; Layout.fillWidth: true }
                    }
                    Card {
                        Layout.fillWidth: true; Layout.leftMargin: 28; Layout.rightMargin: 28
                        implicitHeight: statusText.implicitHeight + 28
                        color: "#211e34"; border.color: "#403453"
                        Caption { id: statusText; anchors.fill: parent; anchors.margins: 14; color: "#c3b3e6"; text: workspace.mockSession ? "MOCK SESSION. Replies come from a deterministic fixture, not a reasoning model. Tasks run through the broker's fixture tools only: no files, applications or networks are touched, and nothing is saved when you close MNAKA." : "No provider connected. Task storage and execution are unavailable. You can start a labelled mock session in Settings." }
                    }
                    ColumnLayout {
                        visible: window.page === 0
                        Layout.fillWidth: true; Layout.leftMargin: 28; Layout.rightMargin: 28; spacing: 14
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "CONVERSATION"; color: window.muted; font.pixelSize: 11; font.letterSpacing: 2 }
                            Item { Layout.fillWidth: true }
                            Button { text: "Clear session"; flat: true; onClicked: clearDialog.open() }
                        }
                        Repeater {
                            model: workspace.messages
                            Card {
                                id: messageCard
                                required property var modelData
                                Layout.fillWidth: true
                                color: messageCard.modelData.role === "user" ? "#192d37" : "#141c2b"
                                implicitHeight: messageColumn.implicitHeight + 32
                                ColumnLayout {
                                    id: messageColumn
                                    anchors.fill: parent; anchors.margins: 16; spacing: 8
                                    Label { text: messageCard.modelData.role === "user" ? "YOU" : messageCard.modelData.mock ? "MNAKA · MOCK RESPONSE" : "MNAKA"; color: messageCard.modelData.role === "user" ? window.cyan : "#b9a1f3"; font.pixelSize: 10; font.weight: Font.Bold }
                                    Label { text: messageCard.modelData.text; textFormat: Text.PlainText; color: window.ink; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 14 }
                                }
                            }
                        }
                        Caption { visible: workspace.busy && workspace.state !== "Awaiting approval"; text: workspace.state === "Cancelling" ? "Cancelling…" : "Preparing response…" }
                        RowLayout {
                            visible: workspace.state === "Awaiting approval"; Layout.fillWidth: true
                            Caption { text: "This task is waiting for your approval."; Layout.fillWidth: true }
                            Button { text: "Review in Task centre"; onClicked: window.page = 1 }
                        }
                        Caption { visible: workspace.streamingText.length > 0; text: workspace.streamingText; Layout.fillWidth: true; color: window.cyan }
                        Caption { visible: workspace.notice.length > 0; text: workspace.notice; Layout.fillWidth: true }
                        Card {
                            Layout.fillWidth: true; implicitHeight: 160
                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 12
                                TextArea {
                                    id: composer; Layout.fillWidth: true; Layout.fillHeight: true
                                    placeholderText: "Ask MNAKA…"; color: window.ink; wrapMode: TextEdit.Wrap
                                    Accessible.name: "Message to MNAKA"
                                    background: Rectangle { color: "transparent" }
                                    Keys.onPressed: event => {
                                        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !(event.modifiers & Qt.ShiftModifier)) {
                                            if (workspace.submit(text, mode.currentText)) text = ""
                                            event.accepted = true
                                        }
                                    }
                                }
                                RowLayout {
                                    Caption { text: "Enter to send · Shift+Enter for a new line"; Layout.fillWidth: true }
                                    Button { text: workspace.busy ? "Cancel" : "Send"; enabled: workspace.busy || composer.text.trim().length > 0; onClicked: { if (workspace.busy) workspace.cancel(); else if (workspace.submit(composer.text, mode.currentText)) composer.text = "" } }
                                }
                            }
                        }
                        Flow {
                            Layout.fillWidth: true; spacing: 8
                            Button { text: "Traffic on the N1"; onClicked: { composer.text = "Mnaka, tell me about traffic on the N1"; composer.forceActiveFocus() } }
                            Button { text: "Inspect a Qt project"; onClicked: { composer.text = "Inspect this Qt project and propose a fix"; composer.forceActiveFocus() } }
                            Button { visible: workspace.mockSession; text: "Approval demo"; onClicked: { composer.text = "Mnaka, run the review demo"; composer.forceActiveFocus() } }
                        }
                        Label { text: "SIX ENGINES · ONE WORKSPACE"; color: window.muted; font.pixelSize: 11; font.letterSpacing: 2; Layout.topMargin: 12 }
                        GridLayout {
                            columns: window.compact ? 1 : 2; columnSpacing: 12; rowSpacing: 12; Layout.fillWidth: true
                            Capability { Layout.fillWidth: true; number: "ENG-01"; name: "Software engineering"; detail: "Scoped patches, isolated builds and review evidence."; destination: 2 }
                            Capability { Layout.fillWidth: true; number: "ENG-02"; name: "Computer control"; detail: "Registered adapters and foreground verification."; destination: 3 }
                            Capability { Layout.fillWidth: true; number: "ENG-03"; name: "Self-extending skills"; detail: "Validated manifests, approval and rollback."; destination: 4 }
                            Capability { Layout.fillWidth: true; number: "ENG-04"; name: "Project intelligence"; detail: "Sourced memories, decisions and deletion."; destination: 5 }
                            Capability { Layout.fillWidth: true; number: "ENG-05"; name: "Vision & spatial"; detail: "Selected images and a bounded scene viewer."; destination: 6 }
                            Capability { Layout.fillWidth: true; number: "ENG-06"; name: "Proactive intelligence"; detail: "Deterministic rules with explicit authorization."; destination: 7 }
                        }
                    }
                    ColumnLayout {
                        visible: window.page === 1; Layout.fillWidth: true; Layout.leftMargin: 28; Layout.rightMargin: 28
                        Card {
                            visible: workspace.approval.id !== undefined
                            Layout.fillWidth: true; implicitHeight: approvalContent.implicitHeight + 32
                            border.color: "#8068b0"
                            ColumnLayout {
                                id: approvalContent; anchors.fill: parent; anchors.margins: 16; spacing: 10
                                Heading { text: "Review this action"; Layout.fillWidth: true }
                                Caption { text: "Simulation only. This approval grants no production permissions."; Layout.fillWidth: true }
                                Label { text: "Tool: " + (workspace.approval.tool || ""); color: window.cyan }
                                Caption { text: "Destination: " + (workspace.approval.destination || "") + "\nEffect: " + (workspace.approval.effect || "") + "\nData leaving device: " + (workspace.approval.dataLeavingDevice || "") + "\nExpires: " + (workspace.approval.expiresAt || ""); Layout.fillWidth: true }
                                Caption { text: "Intent hash: " + (workspace.approval.intentHash || ""); Layout.fillWidth: true; font.pixelSize: 10 }
                                RowLayout {
                                    Button { text: "Approve action"; Accessible.name: "Approve requested action"; onClicked: workspace.decideApproval(true) }
                                    Button { text: "Deny"; onClicked: workspace.decideApproval(false) }
                                }
                            }
                        }
                        Caption { visible: workspace.tasks.length === 0; text: workspace.mockSession ? "No tasks yet. Send a message from Home to create one." : "No tasks. Connect a task service to submit requests."; Layout.fillWidth: true }
                        Repeater {
                            model: workspace.tasks
                            Card {
                                id: taskCard
                                required property var modelData
                                Layout.fillWidth: true; implicitHeight: taskContent.implicitHeight + 32
                                ColumnLayout {
                                    id: taskContent; anchors.fill: parent; anchors.margins: 16; spacing: 8
                                    RowLayout { Layout.fillWidth: true; Label { text: taskCard.modelData.mode; color: window.muted } Item { Layout.fillWidth: true } Pill { text: taskCard.modelData.state } }
                                    Label { text: taskCard.modelData.title; textFormat: Text.PlainText; color: window.ink; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                                    Caption { text: taskCard.modelData.evidence; Layout.fillWidth: true }
                                    Caption { visible: taskCard.modelData.plannedTool !== undefined; text: "Validated plan: " + (taskCard.modelData.plannedTool || ""); Layout.fillWidth: true }
                                    Caption { text: taskCard.modelData.id + " · " + taskCard.modelData.createdAt; Layout.fillWidth: true; font.pixelSize: 10 }
                                    Button { visible: taskCard.modelData.active; text: "Cancel task"; onClicked: workspace.cancel() }
                                }
                            }
                        }
                        Label { visible: workspace.events.length > 0; text: "SESSION EVENTS · NOT PERSISTED"; color: window.muted; font.pixelSize: 11; Layout.topMargin: 14 }
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
                    Card {
                        visible: window.page >= 2 && window.page <= 7
                        Layout.fillWidth: true; Layout.leftMargin: 28; Layout.rightMargin: 28
                        implicitHeight: engineContent.implicitHeight + 48
                        ColumnLayout {
                            id: engineContent; anchors.fill: parent; anchors.margins: 24; spacing: 16
                            Pill { text: "NOT CONNECTED" }
                            Heading { Layout.fillWidth: true; text: ["", "", "A reviewable change starts with a trusted project", "No desktop is paired", "No skills are installed", "No persistent projects or memories", "Vision adapters are not configured", "No proactive rules are active"][window.page] || "" }
                            Caption {
                                Layout.fillWidth: true
                                text: ["", "", "Project picker, indexing, diff, build logs and rollback require the project broker and PostgreSQL. Builds additionally require a tested OS sandbox. Native Windows execution remains disabled.", "No paired devices or application adapters are connected.", "Skills will use registered tools only. Installation requires schema validation, validation tests, a pinned manifest hash and explicit authorization. Generated workflows cannot alter host permissions.", "Facts and decisions will include source, scope, confidence and status. PostgreSQL is the only relational store. No fallback database is created. Export, correction and deletion require the storage service.", "Select a local PNG or JPEG image to preview it. Image interpretation and 3D asset loading are unavailable.", "Planned first rule: weekdays at 08:00, check the recorded build result and suggest a notification on failure. Scheduling needs timezone, quiet hours, cooldown and authorized tool scope."][window.page] || ""
                            }
                            Button { text: "Back to conversation"; onClicked: window.page = 0 }
                        }
                    }
                    Card {
                        visible: window.page === 4; Layout.fillWidth: true; Layout.leftMargin: 28; Layout.rightMargin: 28; implicitHeight: skillContent.implicitHeight + 40
                        ColumnLayout {
                            id: skillContent
                            anchors.fill: parent; anchors.margins: 20; spacing: 12
                            Heading { text: "Inspect a skill manifest" }
                            Caption { text: "Paste your JSON manifest. Checks run locally; inspection grants no permissions."; Layout.fillWidth: true }
                            ScrollView {
                                Layout.fillWidth: true; Layout.preferredHeight: 240
                                TextArea {
                                    id: skillEditor
                                    placeholderText: "Skill manifest JSON"
                                    wrapMode: TextEdit.Wrap
                                    selectByMouse: true
                                    Accessible.name: "Skill manifest JSON"
                                    onTextChanged: workspace.clearSkillReport()
                                }
                            }
                            RowLayout {
                                Button { text: "Validate manifest"; enabled: skillEditor.text.trim().length > 0; onClicked: workspace.inspectSkill(skillEditor.text) }
                                Button { text: "Clear"; onClicked: { skillEditor.clear(); workspace.clearSkillReport() } }
                            }
                            Caption { text: workspace.skillReport; visible: text.length > 0; Layout.fillWidth: true }
                        }
                    }
                    Card {
                        visible: window.page === 6; Layout.fillWidth: true; Layout.leftMargin: 28; Layout.rightMargin: 28; implicitHeight: 420
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 20
                            RowLayout {
                                Button { text: "Select image"; onClicked: workspace.browserImagePicker ? workspace.pickImage() : imagePicker.open() }
                                Button { text: "Clear image"; enabled: workspace.imageLoading || workspace.imagePreview.length > 0; onClicked: workspace.clearImage() }
                                Caption { text: "On this device only"; Layout.fillWidth: true }
                            }
                            Image {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                source: workspace.imagePreview; fillMode: Image.PreserveAspectFit
                                Accessible.name: "Selected image preview"
                            }
                            Caption { text: workspace.imageLoading ? "Loading image…" : workspace.imageNotice.length > 0 ? workspace.imageNotice : "No image selected"; Layout.fillWidth: true }
                        }
                    }
                    Card {
                        visible: window.page === 8; Layout.fillWidth: true; Layout.leftMargin: 28; Layout.rightMargin: 28
                        implicitHeight: settingsContent.implicitHeight + 48
                        ColumnLayout {
                            id: settingsContent; anchors.fill: parent; anchors.margins: 24; spacing: 18
                            Heading { text: "Private by default" }
                            Caption { text: "Camera, audio and location capture are unavailable. Selected images stay in memory on this device. Preferences are session-only. Provider credentials and database connection details are never exposed to QML."; Layout.fillWidth: true }
                            Switch {
                                text: "Mock session (fixture provider)"; checked: workspace.mockSession
                                Accessible.name: "Mock session with fixture provider"
                                onToggled: workspace.mockSession = checked
                            }
                            Caption { text: "Runs typed requests through the task orchestrator, mock model provider and the broker's two fixture tools. Session-only: turning it off or closing MNAKA discards its messages, tasks and events."; Layout.fillWidth: true }
                            Label { text: workspace.mockSession ? "Model provider: MockModelProvider (deterministic fixture)" : "Model provider: not connected"; color: window.cyan }
                            Caption { text: "Cloud transmission: " + (workspace.accountConfigured ? "account sign-in only" : "disabled") + "\nMicrophone / wake detection: unavailable\nCamera / gesture detection: unavailable\nPostgreSQL: " + (Qt.platform.os === "wasm" ? "not part of the browser build" : "desktop adapter built; UI persistence not connected") + "\nBroker: " +(workspace.mockSession ? "fixture tools only (demo.echo, demo.review); no OS access" : "execution disabled") + "\nRelay and mobile pairing: unavailable\nModes affect workflows, never permissions."; Layout.fillWidth: true }
                            Label { text: "ENGINE REGISTRY"; color: window.muted; font.pixelSize: 11 }
                            Repeater {
                                model: workspace.capabilities
                                Caption {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    text: modelData.id + " · " + (modelData.enabled ? "enabled" : "disabled") + " · " + modelData.status
                                }
                            }
                            Button { text: "Delete session history"; onClicked: clearDialog.open() }
                        }
                    }
                    Card {
                        visible: window.page === 8; Layout.fillWidth: true; Layout.leftMargin: 28; Layout.rightMargin: 28
                        implicitHeight: accountContent.implicitHeight + 48
                        ColumnLayout {
                            id: accountContent; anchors.fill: parent; anchors.margins: 24; spacing: 12
                            property bool signedIn: workspace.accountEmail.length > 0
                            Heading { text: "Account" }
                            Caption {
                                Layout.fillWidth: true
                                text: !workspace.accountConfigured ? "No account service is configured for this build."
                                    : accountContent.signedIn ? "Signed in as " + workspace.accountEmail + ". The session is kept in memory only and ends when you close MNAKA. Signing in does not enable any engine, tool or storage."
                                    : "Your email and password are sent to the configured Supabase project over HTTPS. Nothing else leaves this device, and the session is not saved."
                            }
                            TextField {
                                id: accountEmailField; visible: workspace.accountConfigured && !accountContent.signedIn
                                Layout.fillWidth: true; Layout.maximumWidth: 420
                                placeholderText: "Email"; inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                                Accessible.name: "Account email"
                            }
                            TextField {
                                id: accountPasswordField; visible: accountEmailField.visible
                                Layout.fillWidth: true; Layout.maximumWidth: 420
                                placeholderText: "Password"; echoMode: TextInput.Password
                                Accessible.name: "Account password"
                                onAccepted: { workspace.signIn(accountEmailField.text, text); clear() }
                            }
                            RowLayout {
                                visible: accountEmailField.visible
                                Button { text: "Sign in"; enabled: !workspace.accountBusy; onClicked: { workspace.signIn(accountEmailField.text, accountPasswordField.text); accountPasswordField.clear() } }
                                Button { text: "Create account"; enabled: !workspace.accountBusy; onClicked: { workspace.signUp(accountEmailField.text, accountPasswordField.text); accountPasswordField.clear() } }
                            }
                            Button { visible: accountContent.signedIn; text: "Sign out"; onClicked: workspace.signOut() }
                            Caption { text: workspace.accountBusy ? "Contacting the account service…" : workspace.accountNotice; visible: text.length > 0; Layout.fillWidth: true }
                        }
                    }
                    Caption { Layout.leftMargin: 28; Layout.rightMargin: 28; Layout.fillWidth: true; text: "MNAKA v1.1 specification · Foundation preview · Production release gates remain pending"; font.pixelSize: 11 }
                    Item { implicitHeight: 24 }
                }
            }
        }
    }
    Dialog {
        id: clearDialog; anchors.centerIn: parent
        title: "Clear session history?"; modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label { text: "Remove temporary messages and tasks from this session."; color: window.ink; wrapMode: Text.WordWrap; width: Math.min(320, window.width - 80) }
        onAccepted: workspace.clearHistory()
    }
}




