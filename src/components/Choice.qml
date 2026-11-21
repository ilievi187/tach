import QtQuick

// A segmented choice. Material's `highlighted` is too subtle at this size, so
// selection is an accent fill with an accent border, which is unmistakable.
Rectangle {
    id: root

    property string text: ""
    property bool selected: false
    signal clicked()

    implicitHeight: 32
    implicitWidth: 96
    radius: Math.min(6, theme.radius)
    color: selected ? theme.accent : "#171a20"
    border.width: 1
    border.color: selected ? theme.accent : "#242832"

    Behavior on color {
        ColorAnimation { duration: 120 }
    }

    Text {
        anchors.centerIn: parent
        text: root.text
        color: root.selected ? theme.accentForeground : "#c9cfd8"
        font.pixelSize: 12
        font.bold: root.selected
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
