import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import QtQuick.Layouts
import Flint.Flare

AppPage {
    id: page
    required property var session
    required property var presentation
    readonly property bool sensorPanelActive: visible && (!inSwipeView || currentSwipePage) && Window.window !== null && Window.window.visible && Window.window.visibility !== Window.Minimized
    onSensorPanelActiveChanged: sensors.panelActive = sensorPanelActive
    Component.onCompleted: sensors.panelActive = sensorPanelActive
    Component.onDestruction: sensors.panelActive = false
    property int sensorIndex: 0
    readonly property var sensorSource: sensorIndex >= 0 && sensorIndex < sensors.sources.length ? sensors.sources[sensorIndex] : ({})
    function sensorState(state) {
        const language = localization.language;
        return sensors.stateLabel(state);
    }
    function sensorChannel(channel) {
        const language = localization.language;
        return sensors.channelLabel(channel);
    }
    function sourceText(source) {
        let channels = [];
        for (let c = 0; c < 6; ++c)
            if (source.channels & (1 << c)) channels.push(sensorChannel(c));
        return qsTr("Source #%1").arg(source.id) + " · " + channels.join(", ") + (source.simulation ? " · " + qsTr("Simulation") : "");
    }
    Connections {
        target: sensors
        function onChanged() {
            if (page.sensorIndex >= sensors.sources.length) page.sensorIndex = 0;
        }
    }
    BackgroundServicesPanel { panelActive: page.sensorPanelActive }
    AppCard {
        Layout.fillWidth: true
        implicitHeight: sensorContent.implicitHeight + topPadding + bottomPadding
        ColumnLayout {
            id: sensorContent
            anchors.fill: parent
            spacing: 12
            Label { text: qsTr("Sensors"); font.pixelSize: 22; Layout.fillWidth: true }
            Caption {
                text: qsTr("Catalog metadata does not activate sensors. Start diagnostics explicitly to read values. Leaving this panel stops the lease.")
                Layout.fillWidth: true
            }
            AppComboBox {
                id: sensorPicker
                objectName: "sensorSourcePicker"
                Layout.fillWidth: true
                model: sensors.sources.map(source => page.sourceText(source))
                currentIndex: page.sensorIndex
                enabled: !sensors.enabled && !sensors.busy
                onActivated: page.sensorIndex = currentIndex
                Accessible.name: qsTr("Sensor source")
            }
            Caption {
                Layout.fillWidth: true
                visible: sensorPicker.count > 0
                text: qsTr("Cached catalog state: %1 · reason %2 · generation %3 · sample %4 · age %5 ms").arg(page.sensorState(page.sensorSource.state)).arg(page.sensorSource.reason).arg(page.sensorSource.generation).arg(page.sensorSource.sampleId).arg(page.sensorSource.sampleId ? page.sensorSource.age : "—")
            }
            Caption {
                Layout.fillWidth: true
                visible: sensorPicker.count > 0
                text: qsTr("Intervals: %1 / %2 / %3 ms (min/default/max); stale after %4 ms").arg(page.sensorSource.min).arg(page.sensorSource.default).arg(page.sensorSource.max).arg(page.sensorSource.staleAfter)
            }
            Caption {
                Layout.fillWidth: true
                visible: sensorPicker.count > 0
                text: page.sensorSource.temperature === 1 ? qsTr("Ambient temperature") : page.sensorSource.temperature === 2 ? qsTr("Chip temperature; not ambient") : page.sensorSource.coordinates === 1 ? qsTr("Coordinates: device frame") : ""
            }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                AppButton {
                    text: qsTr("Refresh catalog")
                    available: client.connected && !sensors.enabled && !sensors.busy
                    onClicked: sensors.refresh()
                }
                AppButton {
                    objectName: "sensorDiagnosticStart"
                    text: qsTr("Start diagnostics")
                    available: client.connected && sensorPicker.count > 0 && !sensors.enabled && !sensors.busy
                    onClicked: sensors.start(page.sensorSource.id)
                }
                AppButton {
                    objectName: "sensorDiagnosticStop"
                    text: qsTr("Stop diagnostics")
                    available: sensors.enabled || sensors.busy
                    onClicked: sensors.stop()
                }
            }
            Caption { objectName: "sensorDiagnosticMessage"; text: sensors.message; Layout.fillWidth: true }
            Caption {
                Layout.fillWidth: true
                visible: sensors.reading.source !== undefined
                text: qsTr("Last diagnostic sample: %1 · reason %2 · generation %3 · sample %4 · age %5 ms · drops %6").arg(page.sensorState(sensors.reading.status)).arg(sensors.reading.reason).arg(sensors.reading.generation).arg(sensors.reading.sampleId).arg(sensors.reading.sampleId ? sensors.reading.age : "—").arg(sensors.reading.drops)
            }
            Repeater {
                model: sensors.reading.status === 2 ? (sensors.reading.values || []) : []
                delegate: Caption {
                    required property var modelData
                    Layout.fillWidth: true
                    text: page.sensorChannel(modelData.channel) + ": " + Number(modelData.x).toFixed(3) + (modelData.channel >= 4 ? ", " + Number(modelData.y).toFixed(3) + ", " + Number(modelData.z).toFixed(3) : "")
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                visible: client.simulated && sensorPicker.count > 0
                Label { text: qsTr("Virtual sensor controls"); font.bold: true }
                Caption { text: qsTr("Simulation only. Values are published through Core only while a demand exists."); Layout.fillWidth: true }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    AppButton { text: qsTr("Mark unavailable"); onClicked: client.setVirtualSensorAvailable(page.sensorSource.id, false) }
                    AppButton { text: qsTr("Mark available"); onClicked: client.setVirtualSensorAvailable(page.sensorSource.id, true) }
                    AppButton { text: qsTr("Mark pending"); onClicked: client.setVirtualSensorState(page.sensorSource.id, 1) }
                    AppButton { text: qsTr("Mark initializing"); onClicked: client.setVirtualSensorState(page.sensorSource.id, 5) }
                    AppButton { text: qsTr("Mark stale"); onClicked: client.setVirtualSensorState(page.sensorSource.id, 3) }
                    AppButton { text: qsTr("Reset source"); onClicked: client.resetVirtualSensor(page.sensorSource.id) }
                    AppButton { text: qsTr("Advance time by 10 s"); onClicked: client.advanceSimulation(10000) }
                }
                AppComboBox {
                    id: virtualChannel
                    Layout.fillWidth: true
                    property var channelIds: {
                        let ids = [];
                        for (let c = 0; c < 6; ++c) if (page.sensorSource.channels & (1 << c)) ids.push(c);
                        return ids;
                    }
                    model: channelIds.map(c => page.sensorChannel(c))
                    Accessible.name: qsTr("Virtual channel")
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    AppTextField { id: virtualX; width: 110; text: "0"; placeholderText: "X"; validator: DoubleValidator { locale: "C" } Accessible.name: "X" }
                    AppTextField { id: virtualY; width: 110; text: "0"; placeholderText: "Y"; validator: DoubleValidator { locale: "C" } Accessible.name: "Y" }
                    AppTextField { id: virtualZ; width: 110; text: "0"; placeholderText: "Z"; validator: DoubleValidator { locale: "C" } Accessible.name: "Z" }
                    AppButton {
                        text: qsTr("Apply virtual value")
                        available: virtualX.acceptableInput && virtualY.acceptableInput && virtualZ.acceptableInput && virtualChannel.currentIndex >= 0
                        onClicked: client.setVirtualSensorValue(page.sensorSource.id, virtualChannel.channelIds[virtualChannel.currentIndex], Number(virtualX.text), Number(virtualY.text), Number(virtualZ.text))
                    }
                }
            }
        }
    }
    AppCard {
        Layout.fillWidth: true
        implicitHeight: systemContent.implicitHeight + topPadding + bottomPadding
        ColumnLayout {
            id: systemContent
            anchors.fill: parent
            spacing: 12
            Label { text: qsTr("Display and audio"); font.pixelSize: 22 }
            Caption {
                visible: client.simulated
                Layout.fillWidth: true
                text: qsTr("Simulation output: brightness %1% · muted %2").arg(client.virtualBrightness).arg(client.virtualMuted ? qsTr("Yes") : qsTr("No"))
            }
            Caption {
                Layout.fillWidth: true
                text: (sensors.settings.brightness_supported === true) ? qsTr("Confirmed brightness: %1 · manual %2% · fallback %3%").arg(sensors.settings.brightness_mode === "automatic" ? qsTr("Automatic") : qsTr("Manual")).arg(sensors.settings.brightness_manual).arg(sensors.settings.brightness_fallback) : qsTr("Backlight control unavailable")
            }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                AppButton {
                    text: qsTr("Use manual brightness")
                    available: client.connected && (sensors.settings.brightness_supported === true) && !sensors.settingsBusy
                    onClicked: sensors.setSetting("brightness_mode", "manual")
                }
                AppButton {
                    text: qsTr("Use automatic brightness")
                    available: client.connected && (sensors.settings.brightness_supported === true) && !sensors.settingsBusy && sensors.sources.some(source => source.channels & 8)
                    onClicked: sensors.setSetting("brightness_mode", "automatic")
                }
            }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                AppTextField {
                    id: manualBrightness
                    width: 130
                    text: sensors.settings.brightness_manual || "100"
                    validator: IntValidator { bottom: 5; top: 100 }
                    Accessible.name: qsTr("Manual brightness percent")
                    enabled: (sensors.settings.brightness_supported === true) && !sensors.settingsBusy
                }
                AppButton {
                    text: qsTr("Apply manual level")
                    available: client.connected && (sensors.settings.brightness_supported === true) && !sensors.settingsBusy && manualBrightness.acceptableInput
                    onClicked: sensors.setSetting("brightness_manual", manualBrightness.text)
                }
                AppTextField {
                    id: fallbackBrightness
                    width: 130
                    text: sensors.settings.brightness_fallback || "50"
                    validator: IntValidator { bottom: 5; top: 100 }
                    Accessible.name: qsTr("Fallback brightness percent")
                    enabled: (sensors.settings.brightness_supported === true) && !sensors.settingsBusy
                }
                AppButton {
                    text: qsTr("Apply fallback level")
                    available: client.connected && (sensors.settings.brightness_supported === true) && !sensors.settingsBusy && fallbackBrightness.acceptableInput
                    onClicked: sensors.setSetting("brightness_fallback", fallbackBrightness.text)
                }
            }
            AppSwitch {
                Layout.fillWidth: true
                text: (sensors.settings.mute_supported === true) ? qsTr("Mute audio") : qsTr("Audio mute unavailable")
                checked: sensors.settings.audio_muted === "1"
                enabled: client.connected && (sensors.settings.mute_supported === true) && !sensors.settingsBusy
                onToggled: {
                    sensors.setSetting("audio_muted", checked ? "1" : "0");
                    checked = Qt.binding(function() { return sensors.settings.audio_muted === "1"; });
                }
            }
            AppButton {
                text: qsTr("Refresh settings")
                available: client.connected && !sensors.settingsBusy
                onClicked: sensors.refreshSettings()
            }
            Caption { Layout.fillWidth: true; text: sensors.settingsMessage }
        }
    }
    DeviceInfoCard {
        session: page.session
        Layout.fillWidth: true
    }
    WatchPreview {
        session: page.session
        Layout.fillWidth: true
        Layout.preferredHeight: 400
    }
    UsbLogCard {
        session: page.session
        Layout.fillWidth: true
    }
}
