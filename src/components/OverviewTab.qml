import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Format.js" as Fmt

Flickable {
    id: root

    property var app

    contentWidth: width
    contentHeight: content.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ScrollBar.vertical: ScrollBar {
        policy: root.contentHeight > root.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
    }

    ColumnLayout {
        id: content

        width: root.width
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Panel {
                Layout.preferredWidth: 236
                Layout.fillHeight: true
                Layout.minimumHeight: 236

                Item { Layout.fillHeight: true }

                ArcGauge {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 172
                    Layout.preferredHeight: 172
                    value: backend.utilisation
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("GPU load")
                    color: "#8b93a1"
                    font.pixelSize: 13
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("memory controller %1").arg(Fmt.amount(backend.memUtilisation, 0) + " %")
                    color: "#5f6773"
                    font.pixelSize: 11
                }

                Item { Layout.fillHeight: true }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 14

                Panel {
                    title: qsTr("VRAM")
                    hint: qsTr("of %1 GB").arg(Fmt.amount(backend.vramTotalGb, 0))
                    Layout.fillWidth: true

                    Text {
                        text: Fmt.amount(backend.vramUsedGb, 1) + " GB"
                        color: "#f2f4f7"
                        font.pixelSize: 30
                        font.bold: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 8
                        radius: 4
                        color: "#1b1e26"

                        Rectangle {
                            width: parent.width * Math.min(backend.vramFraction, 1)
                            height: parent.height
                            radius: 4
                            color: theme.accent
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    Panel {
                        Layout.fillWidth: true
                        title: qsTr("Junction")

                        Text {
                            text: backend.junctionTemp + "°C"
                            color: backend.junctionTemp >= 100 ? "#ff6b5e" : "#f2f4f7"
                            font.pixelSize: 28
                            font.bold: true
                        }
                        Text {
                            text: qsTr("edge %1 · mem %2").arg(backend.edgeTemp).arg(backend.memTemp)
                            color: "#5f6773"
                            font.pixelSize: 11
                        }
                    }

                    Panel {
                        Layout.fillWidth: true
                        title: qsTr("Power")

                        Text {
                            text: Fmt.amount(backend.power, 0) + " W"
                            color: "#f2f4f7"
                            font.pixelSize: 28
                            font.bold: true
                        }
                        Text {
                            text: backend.powerCapW > 0
                                  ? qsTr("limit %1 W").arg(Fmt.amount(backend.powerCapW, 0))
                                  : qsTr("no limit exposed")
                            color: "#5f6773"
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            title: qsTr("Last minute")

            RowLayout {
                Layout.fillWidth: true
                spacing: 14
                LegendDot { colour: theme.accent; label: qsTr("Load") }
                LegendDot { colour: "#ff8f6b"; label: qsTr("Junction °C") }
                LegendDot { colour: "#7aa2ff"; label: qsTr("Power W") }
                Item { Layout.fillWidth: true }
                Text {
                    text: qsTr("1 sample / second")
                    color: "#5f6773"
                    font.pixelSize: 11
                }
            }

            Chart {
                Layout.fillWidth: true
                Layout.preferredHeight: 128
                utilisation: backend.utilHistory
                temperature: backend.tempHistory
                power: backend.powerHistory
                capacity: 60
                utilisationColor: theme.accent
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Panel {
                Layout.fillWidth: true
                title: qsTr("Core")
                Text {
                    text: Fmt.clock(backend.coreClock)
                    color: "#e8ebef"
                    font.pixelSize: 20
                    font.bold: true
                }
            }
            Panel {
                Layout.fillWidth: true
                title: qsTr("Memory")
                Text {
                    text: Fmt.clock(backend.memClock)
                    color: "#e8ebef"
                    font.pixelSize: 20
                    font.bold: true
                }
            }
            Panel {
                Layout.fillWidth: true
                title: qsTr("Fan")
                Text {
                    text: backend.fanRpm > 0 ? backend.fanRpm + " rpm" : qsTr("off")
                    color: "#e8ebef"
                    font.pixelSize: 20
                    font.bold: true
                }
                Text {
                    text: qsTr("max %1 rpm").arg(backend.fanMax)
                    color: "#5f6773"
                    font.pixelSize: 11
                }
            }
            Panel {
                Layout.fillWidth: true
                title: qsTr("PCIe")
                Text {
                    text: backend.pcie
                    color: "#e8ebef"
                    font.pixelSize: 16
                    font.bold: true
                }
            }
        }
    }
}
