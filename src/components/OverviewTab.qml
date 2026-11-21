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

        Rectangle {
            Layout.fillWidth: true
            visible: backend.junctionTemp >= 95
            implicitHeight: heat.implicitHeight + 24
            radius: theme.radius
            color: theme.dangerSoft
            border.width: 1
            border.color: theme.dangerBorder

            RowLayout {
                id: heat
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Text {
                    text: "▲"
                    color: theme.danger
                    font.pixelSize: 14
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("Junction at %1 °C — the card is near its thermal limit. Lower the power limit or raise the fan curve.").arg(backend.junctionTemp)
                    color: theme.danger
                    font.pixelSize: 12
                }
            }
        }

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
                    color: theme.mutedText
                    font.pixelSize: 13
                }

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("memory controller %1").arg(Fmt.amount(backend.memUtilisation, 0) + " %")
                    color: theme.faintText
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
                        color: theme.foreground
                        font.pixelSize: 30
                        font.bold: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 8
                        radius: 4
                        color: theme.track

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
                            color: backend.junctionTemp >= 100 ? theme.danger : theme.foreground
                            font.pixelSize: 28
                            font.bold: true
                        }
                        Text {
                            text: qsTr("edge %1 · mem %2").arg(backend.edgeTemp).arg(backend.memTemp)
                            color: theme.faintText
                            font.pixelSize: 11
                        }
                    }

                    Panel {
                        Layout.fillWidth: true
                        title: qsTr("Power")

                        Text {
                            text: Fmt.amount(backend.power, 0) + " W"
                            color: theme.foreground
                            font.pixelSize: 28
                            font.bold: true
                        }
                        Text {
                            text: backend.powerCapW > 0
                                  ? qsTr("limit %1 W").arg(Fmt.amount(backend.powerCapW, 0))
                                  : qsTr("no limit exposed")
                            color: theme.faintText
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
                LegendDot { colour: theme.warn; label: qsTr("Junction °C") }
                LegendDot { colour: theme.info; label: qsTr("Power W") }
                Item { Layout.fillWidth: true }
                Text {
                    text: qsTr("1 sample / second")
                    color: theme.faintText
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

        Panel {
            Layout.fillWidth: true
            title: qsTr("Statistics")

            RowLayout {
                Layout.fillWidth: true
                spacing: 28

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: qsTr("Last 60 s")
                        color: theme.faintText
                        font.pixelSize: 11
                    }
                    Text {
                        text: backend.windowStats.utilMax + " %  ·  "
                              + backend.windowStats.tempMax + " °C  ·  "
                              + backend.windowStats.powerMax + " W"
                        color: theme.foreground
                        font.pixelSize: 16
                        font.bold: true
                    }
                }

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: qsTr("Since start")
                        color: theme.faintText
                        font.pixelSize: 11
                    }
                    Text {
                        text: backend.sessionStats.utilMax + " %  ·  "
                              + backend.sessionStats.tempMax + " °C  ·  "
                              + backend.sessionStats.powerMax + " W"
                        color: theme.foreground
                        font.pixelSize: 16
                        font.bold: true
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    Layout.alignment: Qt.AlignBottom
                    text: backend.recording
                          ? qsTr("recording · %1 s").arg(backend.recordedSeconds)
                          : qsTr("%1 samples").arg(backend.sessionStats.samples)
                    color: backend.recording ? theme.danger : theme.faintText
                    font.pixelSize: 11
                }
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
                    color: theme.foreground
                    font.pixelSize: 20
                    font.bold: true
                }
            }
            Panel {
                Layout.fillWidth: true
                title: qsTr("Memory")
                Text {
                    text: Fmt.clock(backend.memClock)
                    color: theme.foreground
                    font.pixelSize: 20
                    font.bold: true
                }
            }
            Panel {
                Layout.fillWidth: true
                title: qsTr("Fan")
                FanMeter {
                    Layout.fillWidth: true
                    rpm: backend.fanRpm
                    maximum: backend.fanMax > 0 ? backend.fanMax : 3600
                }
            }
            Panel {
                Layout.fillWidth: true
                title: qsTr("PCIe")
                Text {
                    text: backend.pcie
                    color: theme.foreground
                    font.pixelSize: 16
                    font.bold: true
                }
            }
        }
    }
}
