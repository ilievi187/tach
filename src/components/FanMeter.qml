import QtQuick
import QtQuick.Layouts
import "Format.js" as Fmt

// The fan as a segmented meter: the number is exact, the lit segments give the
// glance value, and the soft halo makes the active part read at arm's length.
ColumnLayout {
    id: root

    property int rpm: 0
    property int maximum: 3600
    property color accentColor: theme.accent
    property int segments: 18

    spacing: 6

    readonly property real fraction: maximum > 0 ? Math.min(rpm / maximum, 1) : 0
    readonly property int lit: Math.round(fraction * segments)

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        Text {
            text: root.rpm > 0 ? root.rpm.toString() : qsTr("off")
            color: "#e8ebef"
            font.pixelSize: 20
            font.bold: true
        }
        Text {
            Layout.alignment: Qt.AlignBottom
            Layout.bottomMargin: 3
            visible: root.rpm > 0
            text: qsTr("of %1 rpm").arg(root.maximum)
            color: "#5f6773"
            font.pixelSize: 11
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: 12

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3

            Repeater {
                model: root.segments

                delegate: Rectangle {
                    required property int index

                    width: Math.max(2, (root.width - (root.segments - 1) * 3) / root.segments)
                    height: 8
                    radius: 2
                    color: index < root.lit ? root.accentColor : "#1b1e26"

                    Behavior on color {
                        ColorAnimation { duration: 220; easing.type: Easing.OutCubic }
                    }
                }
            }
        }
    }
}
