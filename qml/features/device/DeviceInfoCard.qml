import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Flint.Flare

AppCard {
    id: card
    required property var session
    function bytes(key) {
        const value = session.info[key];
        if (!session.connected || value === undefined || value === null)
            return qsTr("Unavailable");
        if (value < 1024)
            return qsTr("%1 B").arg(value);
        if (value < 1024 * 1024)
            return qsTr("%1 KiB").arg(Number(value / 1024).toLocaleString(Qt.locale(), 'f', 1));
        return qsTr("%1 MiB").arg(Number(value / (1024 * 1024)).toLocaleString(Qt.locale(), 'f', 1));
    }
    implicitHeight: details.implicitHeight + topPadding + bottomPadding
    ColumnLayout {
        id: details
        anchors.fill: parent
        spacing: 14
        RowLayout {
            AppIcon {
                name: "info"
                width: 22
                height: 22
                tint: Theme.accent
            }
            Label {
                text: qsTr("About the device")
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }
        }
        GridLayout {
            Layout.fillWidth: true
            columns: width < 600 ? 1 : 2
            columnSpacing: 16
            rowSpacing: 12
            MetricCard {
                label: qsTr("Device profile")
                detail: card.session.connected ? (card.session.info.profile || qsTr("Unavailable")) : qsTr("Unavailable")
                Layout.fillWidth: true
            }
            MetricCard {
                label: qsTr("Internal RAM budget")
                detail: card.bytes("internal")
                Layout.fillWidth: true
            }
            MetricCard {
                label: qsTr("External RAM budget")
                detail: card.bytes("external")
                Layout.fillWidth: true
            }
            MetricCard {
                label: qsTr("Free storage")
                detail: card.bytes("storage")
                Layout.fillWidth: true
            }
        }
        Caption {
            text: qsTr("Build: %1").arg(card.session.connected ? (card.session.info.build || qsTr("Unavailable")) : qsTr("Unavailable"))
            textFormat: Text.PlainText
            Layout.fillWidth: true
        }
    }
}
