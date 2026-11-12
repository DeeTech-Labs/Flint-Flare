import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import Flint.Flare

MetalSurface {
    id: card
    required property var session
    property bool compact: false
    property bool frameVisible: true
    property int zoom: 0
    objectName: compact ? "overviewPreview" : "watchPreview"
    implicitHeight: Math.max(compact ? 250 : 410, content.implicitHeight + 32)
    implicitWidth: 300
    tone: Theme.surface
    subtle: true
    raised: false
    radius: 12
    FileDialog {
        id: captureDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["PNG (*.png)"]
        defaultSuffix: "png"
        onAccepted: preview.capture(selectedFile)
    }
    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10
        RowLayout {
            Layout.fillWidth: true
            AppIcon {
                name: "watch"
                width: 18
                height: 18
                tint: Theme.accent
            }
            Label {
                text: qsTr("Watch screen")
                Layout.fillWidth: true
                font.weight: Font.DemiBold
            }
            Rectangle {
                width: 6
                height: 6
                radius: 3
                color: card.session.connected ? Theme.accent : Theme.offlineDot
            }
        }
        Image {
            id: screen
            activeFocusOnTab: card.session.simulated && preview.active
            onActiveFocusChanged: preview.setInputFocus(activeFocus)
            Keys.onPressed: event => {
                if (!card.session.simulated || !preview.active || event.isAutoRepeat) return
                if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    preview.sendInput("press", 1)
                    event.accepted = true
                } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                    preview.sendInput("rotate", event.key === Qt.Key_Up ? 1 : -1)
                    event.accepted = true
                }
            }
            Keys.onReleased: event => {
                if (card.session.simulated && preview.active && !event.isAutoRepeat
                        && (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
                    preview.sendInput("release", 0)
                    event.accepted = true
                }
            }
            MouseArea {
                anchors.fill: parent
                enabled: card.session.simulated && preview.active
                onPressed: { screen.forceActiveFocus(); preview.sendInput("press", 1) }
                onReleased: preview.sendInput("release", 0)
                onPressAndHold: preview.sendInput("hold", 1)
                onCanceled: preview.setInputFocus(false)
                onWheel: wheel => {
                    if (wheel.angleDelta.y !== 0) {
                        screen.forceActiveFocus()
                        preview.sendInput("rotate", wheel.angleDelta.y > 0 ? 1 : -1)
                        wheel.accepted = true
                    }
                }
            }
            source: "image://ignite/" + preview.revision
            sourceSize.width: 280
            sourceSize.height: 240
            fillMode: Image.PreserveAspectFit
            cache: false
            smooth: false
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 160
            visible: card.frameVisible
        }
        Caption {
            objectName: "previewStatus"
            text: preview.status || qsTr("Connect a watch or start the local simulator")
            Layout.fillWidth: true
        }
        Caption {
            visible: card.session.simulated && !card.compact
            text: qsTr("Click or press Space to interact. Use Up/Down or the mouse wheel to rotate.")
            Layout.fillWidth: true
        }
        RowLayout {
            visible: !card.compact
            AppButton {
                iconName: "sync"
                text: qsTr("Refresh")
                available: card.session.connected
                onClicked: preview.refresh()
            }
            AppButton {
                iconName: "camera"
                text: qsTr("Save preview")
                available: card.session.connected
                onClicked: captureDialog.open()
            }
        }
    }
}
