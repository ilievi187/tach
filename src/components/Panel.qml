import QtQuick
import QtQuick.Layouts

// The panel every tab builds on: a title, and content laid out inside.
Rectangle {
    id: root

    property string title: ""
    property string hint: ""
    default property alias content: inner.data

    color: theme.panel
    radius: theme.radius
    border.width: 1
    border.color: theme.border
    implicitHeight: layout.implicitHeight + 32
    implicitWidth: 240

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                visible: root.title.length > 0
                text: root.title
                color: theme.mutedText
                font.pixelSize: 11
                font.letterSpacing: 1.1
                font.capitalization: Font.AllUppercase
            }
            Item { Layout.fillWidth: true }
            Text {
                visible: root.hint.length > 0
                text: root.hint
                color: theme.faintText
                font.pixelSize: 11
            }
        }

        ColumnLayout {
            id: inner
            Layout.fillWidth: true
            spacing: 8
        }
    }
}
