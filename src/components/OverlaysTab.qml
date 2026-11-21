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

        Panel {
            Layout.fillWidth: true
            title: qsTr("Detected")
            hint: qsTr("checked at startup")

            Repeater {
                model: ["mangohud", "vkbasalt", "gamescope"]

                delegate: RowLayout {
                    required property string modelData

                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        width: 7
                        height: 7
                        radius: 4
                        color: backend.overlayTools[modelData] ? theme.accent : theme.track
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData
                        color: theme.foreground
                        font.pixelSize: 13
                    }
                    Text {
                        text: backend.overlayTools[modelData] ? qsTr("installed") : qsTr("not installed")
                        color: backend.overlayTools[modelData] ? theme.mutedText : theme.faintText
                        font.pixelSize: 11
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: theme.faintText
                font.pixelSize: 11
                text: qsTr("Install with: sudo pacman -S mangohud vkbasalt gamescope")
            }
        }

        Panel {
            Layout.fillWidth: true
            title: qsTr("MangoHud")
            hint: "~/.config/MangoHud/MangoHud.conf"

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: theme.mutedText
                font.pixelSize: 12
                text: qsTr("Writes a preset into MangoHud's config, inside a marked block that Tach owns. Your own lines are kept, and the old file is backed up first.")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: [
                        { id: "minimal", label: qsTr("Minimal") },
                        { id: "compact", label: qsTr("Compact") },
                        { id: "full", label: qsTr("Full") }
                    ]

                    delegate: Button {
                        required property var modelData

                        Layout.fillWidth: true
                        text: modelData.label
                        onClicked: backend.writeMangoHudPreset(modelData.id)
                    }
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            title: qsTr("vkBasalt")
            hint: "~/.config/vkBasalt/vkBasalt.conf"

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: theme.mutedText
                font.pixelSize: 12
                text: qsTr("Post-processing for Vulkan games. CAS sharpening is the one that pairs with a lower internal resolution; the others are anti-aliasing.")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: [
                        { id: "fxaa", label: "FXAA" },
                        { id: "smaa", label: "SMAA" },
                        { id: "cas", label: "CAS" },
                        { id: "dls", label: "DLS" }
                    ]

                    delegate: Button {
                        required property var modelData

                        Layout.fillWidth: true
                        text: modelData.label
                        onClicked: backend.writeVkBasaltPreset(modelData.id)
                    }
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            title: qsTr("Launch options")
            hint: qsTr("paste into the game's properties")

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: theme.mutedText
                font.pixelSize: 12
                text: qsTr("There is no global FSR switch on Linux: Radeon Super Resolution lives in gamescope, and overlays are switched on per launch. These are the exact strings, copied to the clipboard with one click.")
            }

            Repeater {
                model: [
                    { id: "mangohud", label: qsTr("MangoHud only") },
                    { id: "vkbasalt", label: qsTr("vkBasalt only") },
                    { id: "fsr", label: qsTr("FSR through gamescope") },
                    { id: "everything", label: qsTr("All three") }
                ]

                delegate: ColumnLayout {
                    required property var modelData

                    Layout.fillWidth: true
                    spacing: 4

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: modelData.label
                            color: theme.mutedText
                            font.pixelSize: 11
                        }
                        Button {
                            text: qsTr("Copy")
                            flat: true
                            onClicked: backend.copyToClipboard(backend.launchOptions(modelData.id))
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: backend.launchOptions(modelData.id)
                        color: theme.foreground
                        font.pixelSize: 11
                        font.family: "monospace"
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
