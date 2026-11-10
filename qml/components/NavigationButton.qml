import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Flint.Flare

Button {
    id: control
    property bool selected: false
    property bool compact: false
    property string iconName: "home"
    implicitHeight: 38
    padding: 7
    hoverEnabled: true
    Accessible.role: Accessible.PageTab
    Accessible.name: text
    ToolTip.visible: hovered && compact
    ToolTip.text: text
    contentItem: ColumnLayout {
        spacing: 4
        AppIcon {
            name: control.iconName
            tint: control.selected ? Theme.navSelectedText : control.hovered ? Theme.text : Theme.navText
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            Layout.alignment: Qt.AlignHCenter
        }
        Text {
            font.family: Theme.fontFamily
            visible: !control.compact
            text: control.text
            font.pixelSize: 10
            font.weight: control.selected ? Font.DemiBold : Font.Medium
            color: control.selected ? Theme.navSelectedText : Theme.navText
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
    }
    background: MetalSurface {
        radius: 10
        tone: control.selected ? Theme.navSelected : Theme.navHover
        visible: control.selected || control.hovered || control.visualFocus
        raised: control.selected
        pressed: control.down
        edgeColor: control.visualFocus ? Theme.accent : control.selected ? Theme.metalEdge : Theme.divider
        Behavior on edgeColor {
            ColorAnimation {
                duration: Theme.animationDuration
            }
        }
    }
}
