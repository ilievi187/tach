import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "components"

ApplicationWindow {
    id: win

    width: 1180
    height: 800
    minimumWidth: 360
    minimumHeight: 420
    visible: true
    title: qsTr("Tach — AMD GPU")
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool keysVisible: false
    // Omarchy tiles windows, so a dashboard has to work at whatever width the
    // layout gives it: four columns when there is room, two when there is not,
    // and a scrollbar when the height cannot hold everything.
    readonly property bool compact: width < 900

    function amount(value, decimals) {
        return Number(value).toLocaleString(Qt.locale(), 'f', decimals)
    }

    Shortcut {
        sequence: "?"
        context: Qt.ApplicationShortcut
        onActivated: win.keysVisible = !win.keysVisible
    }
    Shortcut {
        sequence: "Q"
        context: Qt.ApplicationShortcut
        onActivated: Qt.quit()
    }
    Shortcut {
        sequence: "R"
        context: Qt.ApplicationShortcut
        onActivated: backend.refresh()
    }
    Shortcut {
        sequence: "1"
        context: Qt.ApplicationShortcut
        onActivated: backend.setPerformanceLevel("auto")
    }
    Shortcut {
        sequence: "2"
        context: Qt.ApplicationShortcut
        onActivated: backend.setPerformanceLevel("high")
    }
    Shortcut {
        sequence: "3"
        context: Qt.ApplicationShortcut
        onActivated: backend.setPerformanceLevel("low")
    }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        // No card: say so instead of showing empty gauges.
        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 80, 520)
            visible: !backend.available
            spacing: 10

            Text {
                text: qsTr("No AMD GPU found")
                color: "#f2f4f7"
                font.pixelSize: 22
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: backend.error.length > 0 ? backend.error
                                               : qsTr("Tach reads /sys/class/drm. This machine has no AMD card, or the driver is not amdgpu.")
                color: "#9aa1ab"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
        }

        Flickable {
            id: scroller

            anchors.fill: parent
            contentWidth: width
            // Grow to the viewport when there is room, so the cards at the
            // bottom stretch instead of leaving a dead band under them.
            contentHeight: Math.max(content.implicitHeight + 40, scroller.height)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            visible: backend.available

            ScrollBar.vertical: ScrollBar {
                policy: scroller.contentHeight > scroller.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            }

            ColumnLayout {
                id: content

                x: 20
                y: 20
                width: scroller.width - 40
                height: scroller.contentHeight - 40
                spacing: 14

                // ---- header --------------------------------------------
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ColumnLayout {
                        spacing: 0
                        Text {
                            text: qsTr("AMD GPU")
                            color: "#9aa1ab"
                            font.pixelSize: 12
                            font.letterSpacing: 1.2
                            font.capitalization: Font.AllUppercase
                        }
                        Text {
                            Layout.fillWidth: true
                            text: backend.cardName
                            color: "#f2f4f7"
                            font.pixelSize: win.compact ? 20 : 26
                            font.bold: true
                            elide: Text.ElideRight
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        visible: backend.perfLevel.length > 0
                        radius: Math.min(6, theme.radius)
                        color: theme.accent
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: perfLabel.implicitWidth + 20
                        implicitHeight: 26

                        Text {
                            id: perfLabel
                            anchors.centerIn: parent
                            text: backend.perfLevel
                            color: theme.accentForeground
                            font.pixelSize: 12
                            font.bold: true
                        }
                    }
                }

                // ---- gauges --------------------------------------------
                GridLayout {
                    Layout.fillWidth: true
                    columns: win.compact ? 2 : 4
                    columnSpacing: 12
                    rowSpacing: 12

                    Tile {
                        label: qsTr("Utilisation")
                        value: win.amount(backend.utilisation, 0)
                        unit: "%"
                        detail: qsTr("memory %1").arg(win.amount(backend.memUtilisation, 0) + " %")
                        history: backend.utilHistory
                        ceiling: 100
                        showSpark: true
                        valueSize: win.compact ? 26 : 34
                        Layout.preferredHeight: win.compact ? 108 : 124
                    }
                    Tile {
                        label: qsTr("VRAM")
                        value: win.amount(backend.vramUsedGb, 1)
                        unit: qsTr("GB")
                        detail: qsTr("%1 of %2 GB").arg(win.amount(backend.vramUsedGb, 1)).arg(win.amount(backend.vramTotalGb, 1))
                        valueSize: win.compact ? 26 : 34
                        Layout.preferredHeight: win.compact ? 108 : 124
                    }
                    Tile {
                        label: qsTr("Junction")
                        value: backend.junctionTemp.toString()
                        unit: "°C"
                        detail: qsTr("edge %1 · mem %2").arg(backend.edgeTemp).arg(backend.memTemp)
                        history: backend.tempHistory
                        ceiling: 110
                        showSpark: true
                        valueSize: win.compact ? 26 : 34
                        Layout.preferredHeight: win.compact ? 108 : 124
                    }
                    Tile {
                        label: qsTr("Power")
                        value: win.amount(backend.power, 1)
                        unit: "W"
                        detail: qsTr("board draw")
                        history: backend.powerHistory
                        ceiling: 400
                        showSpark: true
                        valueSize: win.compact ? 26 : 34
                        Layout.preferredHeight: win.compact ? 108 : 124
                    }
                }

                // ---- hardware facts -----------------------------------
                GridLayout {
                    Layout.fillWidth: true
                    columns: win.compact ? 2 : 4
                    columnSpacing: 12
                    rowSpacing: 12

                    Tile {
                        Layout.preferredHeight: 92
                        label: qsTr("Core clock")
                        value: backend.coreClock.toString()
                        unit: "MHz"
                        valueSize: win.compact ? 22 : 34
                    }
                    Tile {
                        Layout.preferredHeight: 92
                        label: qsTr("Memory clock")
                        value: backend.memClock.toString()
                        unit: "MHz"
                        valueSize: win.compact ? 22 : 34
                    }
                    Tile {
                        Layout.preferredHeight: 92
                        label: qsTr("Fan")
                        value: backend.fanRpm > 0 ? backend.fanRpm.toString() : qsTr("off")
                        unit: backend.fanRpm > 0 ? "rpm" : ""
                        detail: qsTr("max %1 rpm").arg(backend.fanMax)
                        valueSize: win.compact ? 22 : 34
                    }
                    Tile {
                        Layout.preferredHeight: 92
                        label: qsTr("PCIe link")
                        value: backend.pcie
                        unit: ""
                        valueSize: win.compact ? 16 : 22
                    }
                }

                // ---- processes and control -----------------------------
                GridLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: !win.compact
                    columns: win.compact ? 1 : 2
                    columnSpacing: 12
                    rowSpacing: 12

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: !win.compact
                        Layout.preferredHeight: 240
                        Layout.preferredWidth: 620
                        radius: theme.radius
                        color: "#15171c"
                        border.width: 1
                        border.color: "#23262d"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text {
                                text: qsTr("GPU processes")
                                color: "#f2f4f7"
                                font.pixelSize: 15
                                font.bold: true
                            }
                            Text {
                                visible: backend.processes.length === 0
                                text: qsTr("Nothing is holding VRAM right now.")
                                color: "#9aa1ab"
                                font.pixelSize: 13
                            }

                            Repeater {
                                model: backend.processes

                                delegate: RowLayout {
                                    required property var modelData

                                    Layout.fillWidth: true
                                    spacing: 8

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: "#d7dbe1"
                                        font.pixelSize: 13
                                        elide: Text.ElideMiddle
                                    }
                                    Text {
                                        visible: !win.compact
                                        text: qsTr("pid %1").arg(modelData.pid)
                                        color: "#6f7681"
                                        font.pixelSize: 12
                                    }
                                    Text {
                                        Layout.preferredWidth: 78
                                        horizontalAlignment: Text.AlignRight
                                        text: win.amount(modelData.vram / 1073741824, 2) + " GB"
                                        color: "#9aa1ab"
                                        font.pixelSize: 13
                                    }
                                }
                            }

                            Item { Layout.fillHeight: true }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: win.compact ? 0 : 300
                        Layout.fillWidth: win.compact
                        Layout.preferredHeight: win.compact ? 230 : 0
                        Layout.fillHeight: !win.compact
                        radius: theme.radius
                        color: "#15171c"
                        border.width: 1
                        border.color: "#23262d"

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            Text {
                                text: qsTr("Performance level")
                                color: "#f2f4f7"
                                font.pixelSize: 15
                                font.bold: true
                            }

                            GridLayout {
                                Layout.fillWidth: true
                                columns: win.compact ? 3 : 1
                                columnSpacing: 8
                                rowSpacing: 8

                                Repeater {
                                    model: [
                                        { level: "auto", label: qsTr("Auto") },
                                        { level: "high", label: qsTr("High") },
                                        { level: "low", label: qsTr("Low") }
                                    ]

                                    delegate: Button {
                                        required property var modelData

                                        Layout.fillWidth: true
                                        enabled: backend.canControl && !backend.busy
                                        highlighted: backend.perfLevel === modelData.level
                                        text: modelData.label
                                        onClicked: backend.setPerformanceLevel(modelData.level)
                                    }
                                }
                            }

                            Text {
                                text: qsTr("Changing this asks for your password.")
                                color: "#6f7681"
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }

                            Item { Layout.fillHeight: true }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: backend.status.length > 0 ? backend.status
                                                    : qsTr("R refresh · 1 2 3 set level · ? keys · Q quit")
                    color: backend.busy ? theme.accent : "#6f7681"
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
        }

        // ---- shortcuts ------------------------------------------------
        Rectangle {
            anchors.fill: parent
            visible: win.keysVisible
            color: "#000000cc"

            MouseArea {
                anchors.fill: parent
                onClicked: win.keysVisible = false
            }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(win.width - 60, 420)
                height: keys.implicitHeight + 40
                radius: theme.radius
                color: "#15171c"
                border.width: 1
                border.color: "#2a2e36"

                Column {
                    id: keys
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 10

                    Text {
                        text: qsTr("Keys")
                        color: "#f2f4f7"
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Repeater {
                        model: [
                            { key: "R", action: qsTr("Read a fresh sample") },
                            { key: "1 / 2 / 3", action: qsTr("Performance level: auto, high, low") },
                            { key: "?", action: qsTr("Show or hide this list") },
                            { key: "Esc", action: qsTr("Close") },
                            { key: "Q", action: qsTr("Quit") }
                        ]

                        delegate: Row {
                            required property var modelData
                            width: keys.width
                            spacing: 16

                            Text {
                                width: 90
                                text: modelData.key
                                color: theme.accent
                                font.pixelSize: 13
                                font.bold: true
                            }
                            Text {
                                width: keys.width - 106
                                text: modelData.action
                                color: "#d7dbe1"
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }
}
