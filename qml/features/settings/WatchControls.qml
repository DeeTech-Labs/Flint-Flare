import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Flint.Flare

AppCard {
    objectName: "watchControls"
    id: card
    required property var session
    Dialog {
        id: recovery
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 32)
        modal: true
        title: qsTr("Restore factory components?")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: if (card.session.connected) card.session.recover()
        contentItem: Caption {
            text: qsTr("This will restore the watch’s factory components. Continue?")
        }
    }
    Connections {
        target: card.session
        function onDisconnected() { recovery.close(); }
    }
    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + topPadding + bottomPadding
    ColumnLayout {
        id: content
        anchors.fill: parent
        spacing: 14
        RowLayout {
            AppIcon {
                name: "clock"
                width: 22
                height: 22
                tint: Theme.accent
            }
            Label {
                text: qsTr("Time and display")
                font.pixelSize: 21
                font.weight: Font.DemiBold
            }
        }
        Caption {
            text: qsTr("Set the UTC offset in minutes. Clock, theme and language are provided by Ignite packages.")
            Layout.fillWidth: true
        }
        Flow {
            Layout.fillWidth: true
            Layout.preferredHeight: childrenRect.height
            spacing: 10
            AppTextField {
                id: offset
                width: 200
                text: ""
                placeholderText: qsTr("UTC offset, minutes")
                validator: IntValidator {
                    bottom: -720
                    top: 840
                }
            }
            AppButton {
                iconName: "check"
                text: qsTr("Apply to watch")
                primary: true
                available: card.session.connected && offset.acceptableInput
                onClicked: card.session.setOffset(offset.text)
            }
            AppButton {
                iconName: "sync"
                text: qsTr("Sync time")
                busy: card.session.timeSyncBusy
                available: card.session.connected
                onClicked: card.session.syncTime()
            }
        }
        AppButton {
            iconName: "home"
            text: qsTr("Restore factory components")
            available: card.session.connected
            onClicked: recovery.open()
        }
    }
}
