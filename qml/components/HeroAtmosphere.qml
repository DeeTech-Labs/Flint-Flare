import QtQuick
import QtQuick.Window
import Flint.Flare

Item {
    id: atmosphere
    objectName: "heroAtmosphere"
    property bool active: true
    property real phase: 4.0
    readonly property bool motionRunning: active && visible && Theme.animations && Window.window !== null && Window.window.visible && Window.window.visibility !== Window.Minimized && Window.window.active
    readonly property bool gpuAvailable: GraphicsInfo.api !== GraphicsInfo.Software && GraphicsInfo.api !== GraphicsInfo.Unknown
    // The clock is capped at 30 Hz and pauses off-page, minimized or without focus.
    Timer {
        objectName: "atmosphereClock"
        interval: 33
        repeat: true
        running: atmosphere.motionRunning && atmosphere.gpuAvailable
        onTriggered: atmosphere.phase += 0.033
    }
    // A machined titanium ring also supplies depth with software rendering.
    Item {
        id: sculpture
        anchors.right: parent.right
        anchors.rightMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(560, parent.width * 0.55)
        height: width
        rotation: -28
        opacity: parent.width < 740 ? 0.25 : 0.88
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 10
            anchors.bottomMargin: -10
            radius: width / 2
            color: Theme.titaniumShadow
        }
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            gradient: Gradient {
                GradientStop { position: 0; color: Theme.titaniumDark }
                GradientStop { position: 0.22; color: Theme.titaniumMid }
                GradientStop { position: 0.38; color: Theme.titaniumLight }
                GradientStop { position: 0.44; color: Theme.titaniumSpecular }
                GradientStop { position: 0.52; color: Theme.titaniumMid }
                GradientStop { position: 0.78; color: Theme.titaniumDark }
                GradientStop { position: 1; color: Theme.titaniumMid }
            }
            border.color: Theme.titaniumMid
        }
        Repeater {
            model: 24
            Rectangle {
                required property int index
                anchors.centerIn: parent
                width: parent.width - 4 - index * 3
                height: width
                radius: width / 2
                color: "transparent"
                border.color: index % 2 === 0 ? Theme.titaniumGrooveLight : Theme.titaniumGrooveDark
                border.width: 1
            }
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.68
            height: width
            radius: width / 2
            gradient: Gradient {
                GradientStop { position: 0; color: Theme.titaniumInset }
                GradientStop { position: 0.35; color: Theme.heroEnd }
                GradientStop { position: 1; color: Theme.heroStart }
            }
            border.color: Theme.titaniumDark
            border.width: 3
            Rectangle {
                anchors.fill: parent
                anchors.margins: 4
                radius: width / 2
                color: "transparent"
                border.color: Theme.titaniumLight
                opacity: 0.22
            }
        }
    }
    Loader {
        anchors.fill: parent
        active: atmosphere.active && atmosphere.gpuAvailable
        opacity: 0.28
        sourceComponent: ShaderEffect {
            property vector2d resolution: Qt.vector2d(width, height)
            property color ink: Theme.heroDecoration
            property real phase: atmosphere.phase
            fragmentShader: "qrc:/shaders/atmosphere.frag.qsb"
        }
    }
}
