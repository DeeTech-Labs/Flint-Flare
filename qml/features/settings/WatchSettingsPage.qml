import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Flint.Flare

AppPage {
    id: page
    objectName: "watchPage"
    required property var session
    signal connectRequested
    property bool settingsExpanded: false
    onSessionChanged: settingsExpanded = false
    Connections {
        target: page.session
        function onDisconnected() { page.settingsExpanded = false; }
    }

    Loader {
        objectName: "watchContent"
        Layout.fillWidth: true
        sourceComponent: page.session.connected ? watchDetails : emptyState
    }

    Component {
        id: emptyState
        AppCard {
            objectName: "watchEmptyState"
            implicitHeight: Math.max(440, emptyContent.implicitHeight + 80)
            ColumnLayout {
                id: emptyContent
                anchors.centerIn: parent
                width: Math.min(420, parent.width - 16)
                spacing: 18
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 116
                    height: 116
                    radius: 32
                    color: Theme.metric
                    border.color: Theme.divider
                    AppIcon {
                        anchors.centerIn: parent
                        name: "watch"
                        width: 68
                        height: 68
                        tint: Theme.muted
                    }
                }
                Label {
                    text: page.session.connecting ? qsTr("Connecting to your watch…") : qsTr("No watch connected")
                    font.pixelSize: 28
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Caption {
                    text: qsTr("Connect your FlinT watch to see its information, available storage and active application.")
                    font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
                Caption {
                    visible: page.session.connecting
                    text: page.session.status
                    textFormat: Text.PlainText
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
                AppButton {
                    objectName: "watchConnectButton"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 6
                    iconName: "connection"
                    text: page.session.connecting ? qsTr("View connection") : qsTr("Connect your watch")
                    primary: true
                    onClicked: page.connectRequested()
                }
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 6
                    spacing: 10
                    AppIcon {
                        name: "usb"
                        width: 16
                        height: 16
                        tint: Theme.muted
                    }
                    Caption { text: qsTr("USB or Bluetooth") }
                    AppIcon {
                        name: "bluetooth"
                        width: 16
                        height: 16
                        tint: Theme.muted
                    }
                }
            }
        }
    }

    Component {
        id: watchDetails
        ColumnLayout {
            objectName: "watchDetails"
            spacing: Theme.spacing
            AppCard {
                Layout.fillWidth: true
                implicitHeight: identity.implicitHeight + topPadding + bottomPadding
                ColumnLayout {
                    id: identity
                    anchors.fill: parent
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 14
                        AppIcon {
                            name: "watch"
                            width: 40
                            height: 40
                            tint: Theme.accent
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Label {
                                text: page.session.simulated ? qsTr("Local simulator") : qsTr("My FlinT watch")
                                font.pixelSize: 26
                                font.weight: Font.DemiBold
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                            Caption {
                                text: page.session.simulated ? qsTr("Simulated device · no physical watch connected") : qsTr("Connected via %1").arg(page.session.transport)
                                Layout.fillWidth: true
                            }
                        }
                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: Theme.success
                        }
                    }
                    Flow {
                        Layout.fillWidth: true
                        Layout.preferredHeight: childrenRect.height
                        spacing: 10
                        AppButton {
                            text: qsTr("Refresh information")
                            iconName: "sync"
                            onClicked: page.session.refreshInfo()
                        }
                        AppButton {
                            text: qsTr("Connection")
                            iconName: "connection"
                            onClicked: page.connectRequested()
                        }
                    }
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: width >= 820 ? 2 : 1
                columnSpacing: Theme.spacing
                rowSpacing: Theme.spacing
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: 3
                    Layout.alignment: Qt.AlignTop
                    spacing: Theme.spacing
                    DeviceInfoCard {
                        Layout.fillWidth: true
                        session: page.session
                    }
                    MetricCard {
                        Layout.fillWidth: true
                        label: qsTr("Active application")
                        detail: page.session.info.active || qsTr("Unavailable")
                    }
                }
                WatchPreview {
                    session: page.session
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: 2
                    Layout.preferredHeight: 400
                    frameVisible: !page.inSwipeView || page.currentSwipePage
                }
            }
            AppButton {
                text: page.settingsExpanded ? qsTr("Hide watch settings") : qsTr("Watch settings")
                iconName: page.settingsExpanded ? "down" : "settings"
                onClicked: page.settingsExpanded = !page.settingsExpanded
            }
            AppCard {
                Layout.fillWidth: true
                visible: page.settingsExpanded && page.session.blePolicySupported
                implicitHeight: radioSettings.implicitHeight + topPadding + bottomPadding
                ColumnLayout {
                    id: radioSettings
                    anchors.fill: parent
                    AppSwitch {
                        text: qsTr("Bluetooth availability")
                        checked: page.session.bleEnabled
                        enabled: page.session.blePolicyKnown && !page.session.blePolicyBusy
                        onToggled: page.session.setBleEnabled(checked)
                    }
                    Caption {
                        Layout.fillWidth: true
                        text: qsTr("After disabling Bluetooth, reconnect via USB to enable it. Companion consent is separate.")
                    }
                }
            }
            Loader {
                Layout.fillWidth: true
                active: page.settingsExpanded
                visible: active
                sourceComponent: WatchControls { session: page.session }
            }
        }
    }
}
