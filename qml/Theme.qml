pragma Singleton
import QtQuick
import QtCore

QtObject {
    property int mode: preferences.mode
    onModeChanged: preferences.mode = mode
    property bool developerMode: preferences.developerMode
    onDeveloperModeChanged: preferences.developerMode = developerMode
    property bool animations: preferences.animations
    onAnimationsChanged: preferences.animations = animations
    property int watchTab: navigation.watchTab
    onWatchTabChanged: navigation.watchTab = watchTab
    property int faceTab: navigation.faceTab
    onFaceTabChanged: navigation.faceTab = faceTab
    property int companionTab: navigation.companionTab
    onCompanionTabChanged: navigation.companionTab = companionTab
    property Settings navigation: Settings {
        category: "navigation"
        property int watchTab: 0
        property int faceTab: 0
        property int companionTab: 0
    }
    property Settings preferences: Settings {
        category: "appearance"
        property int mode: 0
        property bool developerMode: false
        property bool animations: true
    }
    readonly property bool light: mode === 1 || (mode === 0 && Qt.styleHints.colorScheme === Qt.Light)
    // Graphite and silver form the shared chrome in both appearance modes.
    readonly property color background: light ? "#cdcdd0" : "#111214"
    readonly property color text: light ? "#27282b" : "#f1f0ed"
    readonly property color muted: light ? "#606168" : "#adaeb3"
    readonly property color accent: light ? "#53565d" : "#c7c8ca"
    readonly property color base: light ? "#e0e0e2" : "#1b1d21"
    readonly property color button: light ? "#d2d3d6" : "#33363d"
    readonly property color accentText: light ? "#ffffff" : "#191b20"
    readonly property color primaryText: accentText
    readonly property color buttonText: light ? "#343a42" : "#e9e8e5"
    readonly property color primaryPressed: light ? "#43464d" : "#a5a7ac"
    readonly property color primaryHover: light ? "#656871" : "#dededc"
    readonly property color buttonHover: light ? "#c6c8cd" : "#41454d"
    readonly property color surface: light ? "#ededef" : "#292c32"
    readonly property color metric: light ? "#d8dade" : "#30333a"
    readonly property color sidebar: background
    readonly property color navText: light ? "#5c636c" : "#a4a6ae"
    readonly property color navSelected: light ? "#c4cad1" : "#c7c8ca"
    readonly property color navSelectedText: "#252a31"
    readonly property color navHover: light ? "#cdd1d7" : "#343a42"
    readonly property color divider: light ? "#c9cdd2" : "#43464d"
    readonly property color offlineDot: light ? "#737b85" : "#949ca7"
    readonly property color success: light ? "#237a5b" : "#70d4a7"
    readonly property color connectedSurface: light ? "#dce7e1" : "#303e37"
    readonly property color offlineSurface: light ? "#dce0e5" : "#353b43"
    // The hero stays graphite in either mode, with silver reflected light.
    readonly property color heroStart: "#3b3e45"
    readonly property color heroEnd: "#191b20"
    readonly property color heroDecoration: "#c5c7cd"
    readonly property color heroHighlight: "#7e838d"
    readonly property color heroTitle: "#f3f4f6"
    readonly property color heroCaption: "#ccced3"
    readonly property color heroText: "#ccced3"
    readonly property color heroFooter: "#b3b6bf"
    readonly property color watchBody: "#292e35"
    readonly property color watchBorder: "#89939f"
    readonly property color watchTime: "#ffffff"
    readonly property color watchScreen: "#111418"
    readonly property color watchCaption: "#b9c1cc"
    readonly property color input: light ? "#d7d9de" : "#181a1f"
    readonly property color inputBorder: light ? "#a0a3ac" : "#4e535e"
    readonly property color error: light ? "#b83849" : "#ff8194"
    readonly property color logText: light ? "#424b57" : "#c2c9d3"
    readonly property color logBackground: light ? "#e2e5e9" : "#1c2025"
    // Shared material lighting: restrained warm silver over cool graphite.
    readonly property color metalEdge: light ? "#a5a8b0" : "#555963"
    readonly property color metalGlint: light ? "#ffffff" : "#c7c9d0"
    readonly property color metalShadow: light ? "#240e1118" : "#70000000"
    readonly property color titaniumLight: "#c9c9c6"
    readonly property color titaniumMid: "#797e88"
    readonly property color titaniumDark: "#30343e"
    readonly property color titaniumSpecular: "#eef0ee"
    readonly property color titaniumGrooveLight: "#20f5f4ed"
    readonly property color titaniumGrooveDark: "#240d1018"
    readonly property color titaniumInset: "#101216"
    readonly property color titaniumShadow: "#80000000"
    readonly property color transparent: "transparent"
    property FontLoader bodyFont: FontLoader {
        source: "qrc:/fonts/Lato-Regular.ttf"
    }
    property FontLoader semiboldFont: FontLoader {
        source: "qrc:/fonts/Lato-Semibold.ttf"
    }
    property FontLoader blackFont: FontLoader {
        source: "qrc:/fonts/Lato-Black.ttf"
    }
    readonly property string fontFamily: bodyFont.name || Qt.application.font.family
    readonly property string monoFamily: Qt.platform.os === "windows" ? "Consolas" : Qt.platform.os === "osx" ? "Menlo" : "monospace"
    readonly property int compactWidth: 900
    readonly property int pageMargin: 20
    readonly property int spacing: 14
    readonly property int cardRadius: 12
    readonly property int controlHeight: 36
    readonly property int animationDuration: animations ? 180 : 0
}
