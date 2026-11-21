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

    GridLayout {
        id: content

        width: root.width
        columns: root.width > 880 ? 2 : 1
        columnSpacing: 14
        rowSpacing: 14

        Panel {
            Layout.fillWidth: true
            Layout.columnSpan: content.columns
            Layout.alignment: Qt.AlignTop
            title: qsTr("Presets")
            hint: backend.activePreset.length > 0 ? backend.activePreset : qsTr("custom")

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: theme.mutedText
                font.pixelSize: 12
                text: qsTr("A preset is a shortcut for the controls below: one click sets the power limit, the DPM level, the power profile and the fan curve together. Change any of them by hand and the chip goes back to custom.")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: [
                        { id: "quiet", label: qsTr("Quiet") },
                        { id: "balanced", label: qsTr("Balanced") },
                        { id: "game", label: qsTr("Game") }
                    ]

                    delegate: Choice {
                        required property var modelData

                        Layout.fillWidth: true
                        selected: backend.activePreset === modelData.id
                        text: modelData.label
                        onClicked: backend.applyPreset(modelData.id)
                    }
                }
            }
        }

        SectionLabel {
            Layout.columnSpan: content.columns
            text: qsTr("Fine tuning")
        }

        Panel {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            title: qsTr("Power limit")
            hint: backend.powerCapW > 0 ? qsTr("now %1 W").arg(Fmt.amount(backend.powerCapW, 0)) : ""

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: theme.mutedText
                font.pixelSize: 12
                text: backend.canPowerCap
                      ? qsTr("The board power ceiling. Lower is quieter and cooler; the card clocks itself to fit. This is the safe knob, unlike voltages.")
                      : qsTr("This GPU does not expose a power limit.")
            }

            Slider {
                id: capSlider

                Layout.fillWidth: true
                visible: backend.canPowerCap
                enabled: !backend.busy
                from: backend.powerCapMinW
                to: backend.powerCapMaxW
                stepSize: 1

                Component.onCompleted: value = backend.powerCapW

                // Follow the card, but never yank the handle while it is held.
                Connections {
                    target: backend
                    function onSampleChanged() {
                        if (!capSlider.pressed)
                            capSlider.value = backend.powerCapW
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: backend.canPowerCap

                Text {
                    text: Fmt.amount(backend.powerCapMinW, 0) + " W"
                    color: theme.faintText
                    font.pixelSize: 11
                }
                Item { Layout.fillWidth: true }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: Fmt.amount(capSlider.value, 0) + " W"
                    color: theme.foreground
                    font.pixelSize: 14
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: Fmt.amount(backend.powerCapMaxW, 0) + " W"
                    color: theme.faintText
                    font.pixelSize: 11
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: backend.canPowerCap

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Default %1 W").arg(Fmt.amount(backend.powerCapDefaultW, 0))
                    color: theme.faintText
                    font.pixelSize: 11
                }
                Button {
                    text: qsTr("Apply")
                    enabled: !backend.busy && Math.abs(capSlider.value - backend.powerCapW) >= 1
                    onClicked: backend.setPowerCap(capSlider.value)
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            title: qsTr("Power profile")
            hint: backend.powerProfile

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: theme.mutedText
                font.pixelSize: 12
                text: qsTr("pp_power_profile_mode: how the driver balances core and memory clocks against the frame rate. Full screen is tuned for games, Video for playback, Power saving for everything else.")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12

                Repeater {
                    model: backend.powerProfiles

                    delegate: RadioButton {
                        required property var modelData

                        Layout.fillWidth: true
                        enabled: !backend.busy
                        checked: backend.powerProfileIndex === modelData.index
                        text: modelData.name
                        onClicked: backend.setPowerProfile(modelData.index)
                    }
                }
            }

            Text {
                visible: backend.powerProfiles.length === 0
                text: qsTr("This GPU does not expose power profiles.")
                color: theme.mutedText
                font.pixelSize: 12
            }
        }

        Panel {
            Layout.fillWidth: true
            Layout.columnSpan: content.columns
            Layout.alignment: Qt.AlignTop
            visible: backend.powerDaemonActive
            title: qsTr("DPM ownership")
            hint: backend.dpmManagedByTach ? qsTr("Tach manages it") : qsTr("shared")

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: theme.mutedText
                font.pixelSize: 12
                text: backend.dpmManagedByTach
                      ? qsTr("power-profiles-daemon no longer touches the amdgpu DPM level, so the performance level above stays where you put it.")
                      : qsTr("power-profiles-daemon is running and may overwrite the performance level with its own profile. Let Tach own the DPM level, or expect the two to fight.")
            }

            RowLayout {
                Layout.fillWidth: true

                Text {
                    Layout.fillWidth: true
                    text: backend.dpmManagedByTach ? qsTr("override active") : qsTr("override not installed")
                    color: theme.faintText
                    font.pixelSize: 11
                }
                Button {
                    text: backend.dpmManagedByTach ? qsTr("Give it back") : qsTr("Let Tach manage it")
                    enabled: !backend.busy
                    onClicked: backend.dpmManagedByTach ? backend.restoreDpm() : backend.takeOverDpm()
                }
            }
        }

        SectionLabel {
            Layout.columnSpan: content.columns
            text: qsTr("Fan")
        }

        FanPanel {
            Layout.fillWidth: true
            Layout.columnSpan: content.columns
            title: ""
        }

        Panel {
            Layout.fillWidth: true
            Layout.columnSpan: content.columns
            Layout.alignment: Qt.AlignTop
            title: qsTr("Overdrive")
            hint: backend.canOverdrive ? qsTr("available") : qsTr("locked by the kernel")

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: theme.mutedText
                font.pixelSize: 12
                text: backend.canOverdrive
                      ? qsTr("This card exposes clock, voltage and fan controls. Tach does not touch them yet: writing pp_od_clk_voltage with a wrong pair of numbers can hang the GPU, and that deserves its own careful pass rather than a slider added in an afternoon.")
                      : qsTr("Clocks, voltage and the fan curve are hidden behind a kernel feature mask. Until it is enabled there is nothing to adjust, so Tach shows no controls that would silently do nothing.")
            }

            RowLayout {
                Layout.fillWidth: true
                visible: !backend.canOverdrive

                Text {
                    Layout.fillWidth: true
                    text: "amdgpu.ppfeaturemask=0xffffffff"
                    color: theme.accent
                    font.pixelSize: 12
                    font.family: "monospace"
                    elide: Text.ElideRight
                }
                Button {
                    text: qsTr("Copy")
                    onClicked: backend.copyToClipboard("amdgpu.ppfeaturemask=0xffffffff")
                }
            }

            Text {
                Layout.fillWidth: true
                visible: !backend.canOverdrive
                text: qsTr("Add it to the kernel command line and reboot, then this section unlocks.")
                color: theme.faintText
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }
}
