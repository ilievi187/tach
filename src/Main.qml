import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "components"

ApplicationWindow {
    id: win

    width: 1180
    height: 800
    minimumWidth: 420
    minimumHeight: 420
    visible: true
    title: qsTr("Tach — AMD GPU")
    color: theme.background

    Material.theme: theme.isLight ? Material.Light : Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool keysVisible: false
    property int tab: 0
    readonly property bool compact: width < 820

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
        onActivated: win.tab = 0
    }
    Shortcut {
        sequence: "2"
        context: Qt.ApplicationShortcut
        onActivated: win.tab = 1
    }
    Shortcut {
        sequence: "3"
        context: Qt.ApplicationShortcut
        onActivated: win.tab = 2
    }
    Shortcut {
        sequence: "4"
        context: Qt.ApplicationShortcut
        onActivated: win.tab = 3
    }
    Shortcut {
        sequence: "5"
        context: Qt.ApplicationShortcut
        onActivated: win.tab = 4
    }

    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft

        // A very slight vertical gradient keeps the flat panels from looking
        // pasted onto a single colour.
        Rectangle {
            anchors.fill: parent
            z: -2
            gradient: Gradient {
                GradientStop { position: 0.0; color: theme.background }
                GradientStop { position: 1.0; color: theme.rail }
            }
        }
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 80, 520)
            visible: !backend.available
            spacing: 10

            Text {
                text: qsTr("No AMD GPU found")
                color: theme.foreground
                font.pixelSize: 22
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: backend.error.length > 0 ? backend.error
                                               : qsTr("Tach reads /sys/class/drm. This machine has no AMD card, or the driver is not amdgpu.")
                color: theme.mutedText
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }
        }

        RowLayout {
            anchors.fill: parent
            spacing: 0
            visible: backend.available

            // ---- rail --------------------------------------------------
            Rectangle {
                Layout.preferredWidth: win.compact ? 76 : 188
                Layout.fillHeight: true
                color: theme.rail

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 4

                    Text {
                        Layout.fillWidth: true
                        text: win.compact ? "T" : "TACH"
                        color: theme.accent
                        font.pixelSize: 18
                        font.bold: true
                        font.letterSpacing: 3
                        Layout.bottomMargin: 18
                    }

                    Repeater {
                        model: [
                            { label: qsTr("Overview"), short: "1" },
                            { label: qsTr("Processes"), short: "2" },
                            { label: qsTr("Tuning"), short: "3" },
                            { label: qsTr("Profiles"), short: "4" },
                            { label: qsTr("Overlays"), short: "5" }
                        ]

                        delegate: Item {
                            id: navItem

                            required property var modelData
                            required property int index

                            Layout.fillWidth: true
                            Layout.preferredHeight: 40

                            HoverHandler { id: navHover }

                            Rectangle {
                                anchors.fill: parent
                                radius: Math.min(6, theme.radius)
                                color: win.tab === index ? theme.panelAlt
                                                         : (navHover.hovered ? theme.hover : "transparent")

                                Behavior on color {
                                    ColorAnimation { duration: 120 }
                                }
                            }
                            Rectangle {
                                width: 3
                                height: 18
                                radius: 2
                                anchors.verticalCenter: parent.verticalCenter
                                color: win.tab === index ? theme.accent : "transparent"
                            }
                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                verticalAlignment: Text.AlignVCenter
                                text: win.compact ? modelData.short : modelData.label
                                color: win.tab === index ? theme.foreground : theme.mutedText
                                font.pixelSize: 14
                                font.bold: win.tab === index
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: win.tab = index
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.bottomMargin: 4
                        visible: !win.compact
                        spacing: 10

                        Repeater {
                            model: [
                                { code: "auto", label: "AUTO" },
                                { code: "bg", label: "BG" },
                                { code: "en", label: "EN" }
                            ]

                            delegate: Text {
                                required property var modelData

                                readonly property bool active: modelData.code === "auto"
                                    ? translator.followingSystem
                                    : (!translator.followingSystem && translator.language === modelData.code)

                                text: modelData.label
                                color: active ? theme.accent : theme.faintText
                                font.pixelSize: 10
                                font.bold: active
                                font.letterSpacing: 0.8

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -4
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: modelData.code === "auto"
                                               ? translator.followSystem()
                                               : translator.setLanguage(modelData.code)
                                }
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        Layout.bottomMargin: 6

                        RowLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            spacing: 6

                            Text {
                                text: "♥"
                                color: theme.danger
                                font.pixelSize: 12
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: !win.compact
                                text: qsTr("Buy me a coffee")
                                color: theme.faintText
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: backend.openUrl("https://buymeacoffee.com/aurorauap")
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: !win.compact
                        text: qsTr("v%1").arg(app_version)
                        color: theme.faintText
                        font.pixelSize: 11
                    }
                }
            }

            // ---- content -----------------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 20
                spacing: 14

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        spacing: 0
                        Text {
                            text: qsTr("AMD GPU")
                            color: theme.faintText
                            font.pixelSize: 10
                            font.letterSpacing: 1.3
                            font.capitalization: Font.AllUppercase
                        }
                        Text {
                            Layout.fillWidth: true
                            text: backend.cardName
                            color: theme.foreground
                            font.pixelSize: win.compact ? 18 : 24
                            font.bold: true
                            elide: Text.ElideRight
                        }
                    }

                    Item { Layout.fillWidth: true }

                    ComboBox {
                        id: cardPicker

                        visible: backend.cards.length > 1
                        Layout.preferredWidth: 178
                        Layout.alignment: Qt.AlignVCenter
                        // "Radeon RX 7900 XT/XTX" is too long for a header control.
                        model: backend.cards.map(function (card) {
                            return card.name.replace("Radeon ", "").replace("RX ", "")
                        })
                        currentIndex: backend.cardIndex
                        onActivated: function (index) { backend.selectCard(index) }
                    }

                    Button {
                        id: recordButton

                        Layout.alignment: Qt.AlignVCenter
                        flat: !backend.recording
                        highlighted: backend.recording
                        onClicked: backend.recording ? backend.stopRecording() : backend.startRecording()

                        contentItem: RowLayout {
                            spacing: 8

                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: theme.danger

                                SequentialAnimation on opacity {
                                    running: backend.recording
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.3; duration: 700; easing.type: Easing.InOutQuad }
                                    NumberAnimation { to: 1.0; duration: 700; easing.type: Easing.InOutQuad }
                                }
                                onOpacityChanged: if (!backend.recording) opacity = 1
                            }

                            Text {
                                text: backend.recording
                                      ? Math.floor(backend.recordedSeconds / 60) + ":"
                                        + ("0" + (backend.recordedSeconds % 60)).slice(-2)
                                      : qsTr("Rec")
                                color: backend.recording ? theme.accentForeground : theme.foreground
                                font.pixelSize: 13
                                font.bold: backend.recording
                            }
                        }
                    }

                    Rectangle {
                        visible: backend.perfLevel.length > 0
                        radius: Math.min(5, theme.radius)
                        color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.12)
                        border.width: 1
                        border.color: Qt.rgba(theme.accent.r, theme.accent.g, theme.accent.b, 0.40)
                        Layout.alignment: Qt.AlignVCenter
                        implicitWidth: perfLabel.implicitWidth + 18
                        implicitHeight: 24

                        Text {
                            id: perfLabel
                            anchors.centerIn: parent
                            text: backend.perfLevel
                            color: theme.accent
                            font.pixelSize: 11
                            font.bold: true
                        }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: win.tab

                    OverviewTab { app: win }
                    ProcessesTab { app: win }
                    TuningTab { app: win }
                    ProfilesTab { app: win }
                    OverlaysTab { app: win }
                }

                Text {
                    Layout.fillWidth: true
                    text: backend.status.length > 0 ? backend.status
                                                    : qsTr("R refresh · 1-5 tabs · ? keys · Q quit")
                    color: backend.busy ? theme.accent : theme.faintText
                    font.pixelSize: 11
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
                width: Math.min(win.width - 60, 440)
                height: keys.implicitHeight + 40
                radius: theme.radius
                color: theme.panel
                border.width: 1
                border.color: theme.border

                Column {
                    id: keys
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 10

                    Text {
                        text: qsTr("Keys")
                        color: theme.foreground
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Repeater {
                        model: [
                            { key: "1 - 5", action: qsTr("Overview, Processes, Tuning, Profiles, Overlays") },
                            { key: "R", action: qsTr("Read a fresh sample") },
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
                                color: theme.foreground
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
