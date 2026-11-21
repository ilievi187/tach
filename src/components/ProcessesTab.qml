import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Format.js" as Fmt

Flickable {
    id: root

    property var app
    property int sortMode: 0   // 0 = VRAM, 1 = GPU time

    contentWidth: width
    contentHeight: content.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ScrollBar.vertical: ScrollBar {
        policy: root.contentHeight > root.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
    }

    // Re-sorting is a view concern: the backend always sends VRAM order, and
    // the GPU-time order is cheap to derive here.
    function rows() {
        const list = backend.processes.slice()
        if (sortMode === 1)
            list.sort(function (a, b) {
                return (b.busy || 0) - (a.busy || 0)
            })
        return list
    }

    ColumnLayout {
        id: content

        width: root.width
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: qsTr("By %1").arg(root.sortMode === 0 ? qsTr("VRAM") : qsTr("GPU time"))
                color: "#8b93a1"
                font.pixelSize: 11
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("VRAM")
                flat: true
                highlighted: root.sortMode === 0
                onClicked: root.sortMode = 0
            }
            Button {
                text: qsTr("GPU time")
                flat: true
                highlighted: root.sortMode === 1
                onClicked: root.sortMode = 1
            }
        }

        Panel {
            Layout.fillWidth: true
            title: qsTr("Processes")
            hint: qsTr("refreshed every 5 s")

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 2
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Name")
                    color: "#5f6773"
                    font.pixelSize: 10
                    font.capitalization: Font.AllUppercase
                }
                Text {
                    Layout.preferredWidth: 70
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("GPU")
                    color: "#5f6773"
                    font.pixelSize: 10
                    font.capitalization: Font.AllUppercase
                }
                Text {
                    Layout.preferredWidth: 96
                    horizontalAlignment: Text.AlignRight
                    text: qsTr("VRAM")
                    color: "#5f6773"
                    font.pixelSize: 10
                    font.capitalization: Font.AllUppercase
                }
            }

            Repeater {
                model: root.rows()

                delegate: Item {
                    id: processRow
                    required property var modelData

                    Layout.fillWidth: true
                    implicitHeight: rowLayout.implicitHeight + 10

                    HoverHandler { id: rowHover }

                    Rectangle {
                        anchors.fill: parent
                        radius: Math.min(4, theme.radius)
                        color: rowHover.hovered ? "#181b22" : "transparent"
                    }

                    RowLayout {
                        id: rowLayout
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 10

                        Text {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: "#d7dbe1"
                            font.pixelSize: 13
                            elide: Text.ElideMiddle
                        }
                        Text {
                            Layout.preferredWidth: 70
                            horizontalAlignment: Text.AlignRight
                            text: modelData.busy > 0.5 ? Fmt.amount(modelData.busy, 0) + " %" : "—"
                            color: modelData.busy > 50 ? theme.accent : "#8b93a1"
                            font.pixelSize: 12
                        }
                        Text {
                            Layout.preferredWidth: 96
                            horizontalAlignment: Text.AlignRight
                            text: Fmt.amount(modelData.vram / 1073741824, 2) + " GB"
                            color: "#8b93a1"
                            font.pixelSize: 12
                        }
                    }

                }
            }

            Text {
                visible: backend.processes.length === 0
                Layout.fillWidth: true
                text: qsTr("Nothing is holding VRAM right now.")
                color: "#8b93a1"
                font.pixelSize: 13
            }
        }

        Panel {
            Layout.fillWidth: true
            title: qsTr("Where this comes from")

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#8b93a1"
                font.pixelSize: 12
                text: qsTr("VRAM is drm-total-vram from /proc/<pid>/fdinfo, summed per DRM client so a process with several descriptors is not counted twice. GPU time is the change in drm-engine-gfx between two samples.")
            }
        }
    }
}
