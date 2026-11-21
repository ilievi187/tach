import QtQuick

// A segmented choice. Selection is a soft accent tint with accent text, not a
// filled block: three bright fills in one column read as a warning banner
// rather than a control.
Rectangle {
    id: root

    property string text: ""
    property bool selected: false
    signal clicked()

    implicitHeight: 32
    implicitWidth: 96
    radius: Math.min(6, theme.radius)
    color: selected ? Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.13)
                    : "transparent"
    border.width: 1
    border.color: selected ? Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.45)
                           : "#242832"

    Behavior on color {
        ColorAnimation { duration: 140 }
    }

    Text {
        anchors.centerIn: parent
        text: root.text
        color: root.selected ? theme.accent : "#a7aeb9"
        font.pixelSize: 12
        font.bold: root.selected
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
