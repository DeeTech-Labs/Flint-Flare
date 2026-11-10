import QtQuick
import Flint.Flare

Rectangle {
    id: material
    property color tone: Theme.surface
    property bool raised: true
    property bool pressed: false
    property color edgeColor: Theme.metalEdge
    radius: Theme.cardRadius
    gradient: Gradient {
        GradientStop { position: 0; color: Qt.lighter(material.tone, material.pressed ? 1.02 : 1.12) }
        GradientStop { position: 0.20; color: Qt.lighter(material.tone, 1.04) }
        GradientStop { position: 0.55; color: material.tone }
        GradientStop { position: 0.88; color: Qt.darker(material.tone, 1.06) }
        GradientStop { position: 1; color: Qt.darker(material.tone, 1.10) }
    }
    border.color: edgeColor
    border.width: 1
    Behavior on tone { ColorAnimation { duration: Theme.animationDuration } }
    Rectangle {
        z: -1
        visible: material.raised && !material.pressed
        x: 0
        y: 2
        width: parent.width
        height: parent.height
        radius: material.radius
        color: Theme.metalShadow
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Math.max(0, material.radius - 1)
        color: "transparent"
        border.color: Theme.metalGlint
        opacity: Theme.light ? 0.48 : 0.10
    }
    Rectangle {
        x: material.radius
        y: 1
        width: Math.max(0, parent.width - material.radius * 2)
        height: 1
        color: Theme.metalGlint
        opacity: material.pressed ? 0.08 : Theme.light ? 0.55 : 0.18
    }
}
