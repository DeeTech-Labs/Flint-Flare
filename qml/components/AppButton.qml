import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Flint.Flare

Button {
    id: action
    property bool primary: false
    property string iconName: ""
    property bool available: true
    property bool busy: false
    property string accessibleName: text
    enabled: available && !busy
    Accessible.name: accessibleName
    ToolTip.text: accessibleName
    ToolTip.visible: hovered && text.length === 0 && accessibleName.length > 0
    ToolTip.delay: 500
    activeFocusOnTab: true
    implicitHeight: Theme.controlHeight
    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    leftPadding: text.length ? 12 : 10
    rightPadding: text.length ? 12 : 10
    hoverEnabled: true
    contentItem: Row {
        spacing: action.iconName ? 9 : 0
        opacity: action.enabled ? 1 : 0.45
        AppIcon {
            visible: action.iconName !== ""
            name: action.iconName
            width: visible ? 16 : 0
            height: 16
            anchors.verticalCenter: parent.verticalCenter
            tint: action.primary ? Theme.primaryText : Theme.buttonText
        }
        Text {
            font.family: Theme.fontFamily
            text: action.text
            textFormat: Text.PlainText
            font.pixelSize: 13
            font.weight: Font.DemiBold
            color: action.primary ? Theme.primaryText : Theme.buttonText
            height: parent.height
            verticalAlignment: Text.AlignVCenter
        }
    }
    background: MetalSurface {
        radius: 9
        edgeColor: action.visualFocus ? Theme.accent : action.primary ? Qt.darker(Theme.accent, 1.18) : Theme.metalEdge
        tone: action.primary ? (action.down ? Theme.primaryPressed : action.hovered ? Theme.primaryHover : Theme.accent) : (action.down ? Theme.metric : action.hovered ? Theme.buttonHover : Theme.button)
        pressed: action.down
        opacity: action.enabled ? 1 : 0.6
    }
}
