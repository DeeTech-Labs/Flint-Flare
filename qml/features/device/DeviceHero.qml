import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Flint.Flare

Rectangle {
    id: hero
    required property var session
    required property var presentation
    property bool frameVisible: true
    readonly property bool narrow: width < 740
    readonly property int titlePixelSize: narrow ? 28 : 32
    signal syncRequested
    signal settingsRequested
    signal connectRequested
    implicitHeight: Math.max(200, words.implicitHeight + 40)
    radius: 18
    clip: true
    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop { position: 0; color: Theme.heroStart }
        GradientStop { position: 1; color: Theme.heroEnd }
    }
    border.color: Theme.metalEdge
    border.width: 1
    HeroAtmosphere {
        anchors.fill: parent
        active: hero.frameVisible
    }
    ColumnLayout {
        id: words
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 20
        width: hero.narrow ? parent.width - 40 : parent.width * 0.62
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle {
                width: 6
                height: 6
                radius: 3
                color: hero.session.connected ? Theme.success : Theme.offlineDot
            }
            Label {
                text: hero.session.connected ? (hero.session.simulated ? qsTr("Simulator connected") : qsTr("Connected via %1").arg(hero.session.transport)) : hero.presentation.stateLabel
                textFormat: Text.PlainText
                font.pixelSize: 14
                color: Theme.heroCaption
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }
        Label {
            text: hero.session.connected ? (hero.session.simulated ? qsTr("Local simulator") : qsTr("Your watch")) : qsTr("Connect your watch")
            textFormat: Text.PlainText
            font.pixelSize: hero.titlePixelSize
            font.weight: Font.DemiBold
            font.letterSpacing: -0.02 * hero.titlePixelSize
            lineHeight: 1.05
            color: Theme.heroTitle
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
        Label {
            text: hero.session.connected ? qsTr("Device profile: %1").arg(hero.session.info.profile || qsTr("Unavailable")) : qsTr("Connect via USB or Bluetooth, or try the simulator.")
            textFormat: Text.PlainText
            font.pixelSize: 14
            color: Theme.heroText
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
        Flow {
            Layout.fillWidth: true
            Layout.preferredHeight: childrenRect.height
            Layout.topMargin: 4
            spacing: 10
            AppButton {
                visible: hero.session.connected
                iconName: "settings"
                text: qsTr("Watch settings")
                primary: true
                onClicked: hero.settingsRequested()
            }
            AppButton {
                visible: !hero.session.connected
                iconName: "connection"
                text: qsTr("Connect your watch")
                primary: true
                onClicked: hero.connectRequested()
            }
            AppButton {
                objectName: "syncTimeButton"
                visible: hero.session.connected
                iconName: "sync"
                text: hero.session.timeSyncBusy ? qsTr("Syncing…") : qsTr("Sync time")
                busy: hero.session.timeSyncBusy
                available: hero.session.connected
                onClicked: hero.syncRequested()
            }
        }
        Label {
            objectName: "timeSyncFeedback"
            visible: hero.session.connected && (hero.session.timeSyncBusy || hero.session.timeSynced || hero.session.timeSyncError.length > 0)
            text: hero.session.timeSyncBusy ? qsTr("Synchronizing time…") : hero.session.timeSyncError.length > 0 ? qsTr("Time sync failed: %1").arg(hero.session.timeSyncError) : hero.session.timeSynced ? qsTr("Time synchronized") : ""
            textFormat: Text.PlainText
            font.pixelSize: 13
            color: hero.session.timeSyncError.length > 0 ? Theme.error : Theme.heroText
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.StaticText
            Accessible.name: text
        }
    }
}
