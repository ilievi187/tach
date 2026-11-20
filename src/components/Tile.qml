import QtQuick
import QtQuick.Layouts

// One metric: name, big value, unit, an optional second line, and an optional
// history line underneath.
Rectangle {
    id: tile

    property string label: ""
    property string value: ""
    property string unit: ""
    property string detail: ""
    property var history: []
    property real ceiling: 100
    property color accentColor: theme.accent
    property bool showSpark: false
    property int valueSize: 34

    Layout.fillWidth: true
    Layout.preferredHeight: 124
    implicitWidth: 200
    radius: theme.radius
    color: "#15171c"
    border.width: 1
    border.color: "#23262d"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 2

        Text {
            text: tile.label
            color: "#9aa1ab"
            font.pixelSize: 12
            font.letterSpacing: 0.6
            font.capitalization: Font.AllUppercase
            elide: Text.ElideRight
        }

        Text {
            textFormat: Text.StyledText
            text: tile.unit.length > 0
                  ? tile.value + " <span style='font-size:" + Math.round(tile.valueSize * 0.45)
                    + "px; color:#9aa1ab'>" + tile.unit + "</span>"
                  : tile.value
            color: "#f2f4f7"
            font.pixelSize: tile.valueSize
            font.bold: true
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Text {
            text: tile.detail
            visible: tile.detail.length > 0
            color: "#9aa1ab"
            font.pixelSize: 12
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }

        Sparkline {
            visible: tile.showSpark
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            values: tile.history
            ceiling: tile.ceiling
            lineColor: tile.accentColor
        }
    }
}
