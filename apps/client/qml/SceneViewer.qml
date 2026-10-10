pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick3D

Item {
    id: viewer
    objectName: "sampleSceneViewer"
    property real yaw: 20
    property real zoom: 1
    property bool selected: false
    function probeSelection(): bool { return scene.pick(width / 2, height / 2).objectHit === sample }
    View3D {
        id: scene
        anchors.fill: parent
        camera: sceneCamera
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Color
            clearColor: "#111b2a"
        }
        PerspectiveCamera {
            id: sceneCamera
            position: Qt.vector3d(0, 80 / viewer.zoom, 360 / viewer.zoom)
            eulerRotation.x: -12
            clipNear: 1
            clipFar: 2000
        }
        DirectionalLight { eulerRotation: Qt.vector3d(-35, -30, 0); brightness: 1.2 }
        DirectionalLight { eulerRotation: Qt.vector3d(20, 140, 0); brightness: 0.7; color: "#ae94ef" }
        Model {
            id: sample
            source: "#Cube"
            pickable: true
            eulerRotation: Qt.vector3d(12, viewer.yaw, 0)
            materials: PrincipledMaterial {
                baseColor: viewer.selected ? "#b59af6" : "#64e4d1"
                metalness: 0.2
                roughness: 0.35
            }
        }
        Model {
            source: "#Rectangle"
            position.y: -85
            eulerRotation.x: -90
            scale: Qt.vector3d(4, 4, 1)
            materials: PrincipledMaterial { baseColor: "#25374a"; roughness: 1 }
        }
        MouseArea {
            anchors.fill: parent
            onClicked: mouse => viewer.selected = scene.pick(mouse.x, mouse.y).objectHit === sample
        }
    }
    Label {
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 12
        text: viewer.selected ? "Sample cube selected" : "Click the sample cube to select"
        color: "#e4edf8"; font.pixelSize: 12
        background: Rectangle { color: "#cc111b2a"; radius: 4 }
    }
    Button {
        anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8
        text: viewer.selected ? "Deselect" : "Select sample"
        Accessible.name: "Toggle sample cube selection"
        onClicked: viewer.selected = !viewer.selected
    }
}
