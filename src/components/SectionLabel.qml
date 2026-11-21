import QtQuick
import QtQuick.Layouts

// A quiet divider between "apply a bundle" and "adjust the parts".
RowLayout {
    id: root

    property string text: ""

    Layout.fillWidth: true
    Layout.topMargin: 4
    Layout.bottomMargin: 0
    spacing: 10

    Text {
        text: root.text
        color: theme.faintText
        font.pixelSize: 10
        font.letterSpacing: 1.2
        font.capitalization: Font.AllUppercase
    }

    Rectangle {
        Layout.fillWidth: true
        height: 1
        color: theme.border
    }
}
