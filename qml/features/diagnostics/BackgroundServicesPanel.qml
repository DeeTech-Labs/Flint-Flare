import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Flint.Flare

AppCard {
    id: panel
    required property bool panelActive
    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + topPadding + bottomPadding
    onPanelActiveChanged: { backgroundServices.panelActive = panelActive; if (!panelActive) consent.close(); }
    Component.onCompleted: backgroundServices.panelActive = panelActive
    Component.onDestruction: backgroundServices.panelActive = false
    function stateName(state) {
        const language = localization.language;
        return backgroundServices.stateLabel(state);
    }
    function reasonName(reason) {
        const language = localization.language;
        return backgroundServices.reasonLabel(reason);
    }
    Dialog {
        id: consent
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(500, parent.width - 32)
        modal: true
        title: qsTr("Background service permission")
        property string packageId
        property string permission
        property bool enable
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label {
            text: consent.packageId + "\n" + hub.permissionLabel(consent.permission) + "\n\n" + (consent.enable ? qsTr("Allow this permission? Starting a service does not grant it automatically.") : qsTr("Revoke this permission?"))
            wrapMode: Text.WordWrap
            textFormat: Text.PlainText
        }
        onAccepted: if (panel.panelActive && client.connected) backgroundServices.grant(packageId, permission, enable)
    }
    Connections { target: client; function onDisconnected() { consent.close(); } }
    onVisibleChanged: if (!visible) consent.close()
    ColumnLayout {
        id: content
        anchors.fill: parent
        spacing: 12
        Label { text: qsTr("Background services"); font.pixelSize: 22; Layout.fillWidth: true }
        Caption {
            Layout.fillWidth: true
            text: qsTr("Services run in Core independently of this panel. Closing it or disconnecting Flare stops monitoring, not device services. No application data is displayed.")
        }
        Caption {
            Layout.fillWidth: true
            visible: backgroundServices.policy.maxServices !== undefined
            text: qsTr("Policy: %1 services · %2 bytes · %3 instructions and %4 native calls per %5 ms · low energy: %6").arg(backgroundServices.policy.maxServices).arg(backgroundServices.policy.memoryBytes).arg(backgroundServices.policy.instructionsPerWindow).arg(backgroundServices.policy.nativeCallsPerWindow).arg(backgroundServices.policy.windowMs).arg(backgroundServices.policy.lowEnergy ? qsTr("Yes") : qsTr("No"))
        }
        AppSwitch {
            visible: client.simulated
            Layout.fillWidth: true
            text: qsTr("Simulate low energy")
            checked: client.virtualLowEnergy
            enabled: client.connected
            onToggled: client.virtualLowEnergy = checked
        }
        AppButton {
            text: qsTr("Refresh services")
            available: client.connected && !backgroundServices.busy
            onClicked: backgroundServices.refresh()
        }
        Caption { text: backgroundServices.message; textFormat: Text.PlainText; Layout.fillWidth: true }
        Caption { visible: client.connected && backgroundServices.policy.maxServices !== undefined && backgroundServices.services.length === 0; text: qsTr("No installed background services"); Layout.fillWidth: true }
        Repeater {
            model: backgroundServices.model
            delegate: ColumnLayout {
                id: row
                required property var record
                Layout.fillWidth: true
                spacing: 8
                Label {
                    text: (row.record.label || row.record.component) + " · " + row.record.package + "/" + row.record.component
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                    font.bold: true
                }
                Caption {
                    text: qsTr("%1 · %2 · error %3 · instance %4 / %5").arg(panel.stateName(row.record.state)).arg(panel.reasonName(row.record.reason)).arg(row.record.error).arg(row.record.instanceId).arg(row.record.generation)
                    Layout.fillWidth: true
                }
                Caption {
                    text: qsTr("Memory used/peak/reserved: %1 / %2 / %3 bytes · batches %4 · instructions %5 · native calls %6").arg(row.record.used).arg(row.record.peak).arg(row.record.reserved).arg(row.record.callbacks).arg(row.record.instructions).arg(row.record.nativeCalls)
                    Layout.fillWidth: true
                }
                Caption {
                    text: qsTr("Max dispatch batch: %1 µs · dispatches %2 · rejected/coalesced events %3 / %4").arg(row.record.maxCallbackUs).arg(row.record.wakeups).arg(row.record.eventRejected).arg(row.record.eventCoalesced)
                    Layout.fillWidth: true
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    AppButton {
                        text: qsTr("Start")
                        available: client.connected && !backgroundServices.updating && row.record.executeGranted && backgroundServices.policy.maxServices > 0 && (row.record.state === 0 || row.record.state === 4)
                        onClicked: backgroundServices.control(0, row.record.package, row.record.component)
                    }
                    AppButton {
                        text: qsTr("Stop")
                        available: client.connected && !backgroundServices.updating && row.record.state !== 0
                        onClicked: backgroundServices.control(1, row.record.package, row.record.component)
                    }
                    AppButton {
                        text: row.record.state === 3 ? qsTr("Resume") : qsTr("Pause")
                        available: client.connected && !backgroundServices.updating && (row.record.state === 2 || row.record.state === 3)
                        onClicked: backgroundServices.control(row.record.state === 3 ? 3 : 2, row.record.package, row.record.component)
                    }
                    AppButton {
                        text: row.record.executeGranted ? qsTr("Revoke execution") : qsTr("Allow execution…")
                        available: client.connected && !backgroundServices.updating
                        onClicked: {
                            if (row.record.executeGranted) backgroundServices.control(6, row.record.package, row.record.component);
                            else { consent.packageId = row.record.package; consent.permission = "app.background.execute"; consent.enable = true; consent.open(); }
                        }
                    }
                    AppButton {
                        text: row.record.autostartGranted ? qsTr("Revoke automatic start permission…") : qsTr("Allow automatic start permission…")
                        available: client.connected && !backgroundServices.updating
                        onClicked: { consent.packageId = row.record.package; consent.permission = "app.background.autostart"; consent.enable = !row.record.autostartGranted; consent.open(); }
                    }
                }
                AppSwitch {
                    Layout.fillWidth: true
                    text: qsTr("Start this service automatically")
                    checked: row.record.autostart
                    enabled: client.connected && !backgroundServices.updating && row.record.executeGranted && row.record.autostartGranted
                    onToggled: {
                        backgroundServices.control(checked ? 4 : 5, row.record.package, row.record.component);
                        checked = Qt.binding(function() { return row.record.autostart; });
                    }
                }
            }
        }
    }
}
