import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    property color colour: theme.foreground
    property string label: ""

    spacing: 6

    Rectangle {
        Layout.alignment: Qt.AlignVCenter
        width: 8
        height: 8
        radius: 4
        color: root.colour
    }
    Text {
        Layout.alignment: Qt.AlignVCenter
        text: root.label
        color: theme.mutedText
        font.pixelSize: 11
    }
}
