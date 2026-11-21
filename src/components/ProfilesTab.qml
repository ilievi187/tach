import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: root

    property var app
    property bool autoSwitch: false
    property string fallback: "balanced"
    property var rules: []

    readonly property var presets: [
        { id: "quiet", label: qsTr("Quiet") },
        { id: "balanced", label: qsTr("Balanced") },
        { id: "game", label: qsTr("Game") }
    ]

    function presetLabel(id) {
        for (let i = 0; i < presets.length; i++)
            if (presets[i].id === id)
                return presets[i].label
        return id
    }

    function presetIndex(id) {
        for (let i = 0; i < presets.length; i++)
            if (presets[i].id === id)
                return i
        return 1
    }

    function addRule() {
        const next = rules.slice()
        next.push({ match: "", preset: "game" })
        rules = next
    }

    function updateRule(index, key, value) {
        const next = rules.slice()
        const copy = Object.assign({}, next[index])
        copy[key] = value
        next[index] = copy
        rules = next
    }

    function removeRule(index) {
        const next = rules.slice()
        next.splice(index, 1)
        rules = next
    }

    Component.onCompleted: {
        autoSwitch = backend.profileAuto
        fallback = backend.profileDefault
        rules = backend.profileRules.slice()
    }

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
            title: qsTr("Automatic switching")
            hint: backend.profilesServiceActive ? qsTr("service active") : qsTr("service stopped")

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Switch {
                    id: autoSwitchControl
                    checked: root.autoSwitch
                    onToggled: root.autoSwitch = checked
                }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: "#8b93a1"
                    font.pixelSize: 12
                    text: qsTr("A small service watches what is running and applies the matching preset: start a game and the card goes to 3D_FULL_SCREEN with the fan curve that suits it, close it and the default comes back. It runs as a system service so it never asks for a password mid-game.")
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            title: qsTr("Default")
            hint: qsTr("when nothing matches")

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: root.presets

                    delegate: Choice {
                        required property var modelData

                        Layout.fillWidth: true
                        text: modelData.label
                        selected: root.fallback === modelData.id
                        onClicked: root.fallback = modelData.id
                    }
                }
            }
        }

        Panel {
            Layout.fillWidth: true
            title: qsTr("Rules")
            hint: qsTr("%1").arg(root.rules.length)

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#5f6773"
                font.pixelSize: 11
                text: qsTr("Match is a piece of the process name, case-insensitive. \"steam_app_\" catches every Proton game; \"Cyberpunk2077\" is narrower.")
            }

            Repeater {
                model: root.rules

                delegate: RowLayout {
                    id: ruleRow
                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    spacing: 8

                    TextField {
                        Layout.fillWidth: true
                        text: ruleRow.modelData.match
                        placeholderText: qsTr("process name, e.g. steam_app_")
                        onEditingFinished: root.updateRule(ruleRow.index, "match", text)
                    }

                    ComboBox {
                        Layout.preferredWidth: 150
                        model: root.presets
                        textRole: "label"
                        currentIndex: root.presetIndex(ruleRow.modelData.preset)
                        onActivated: function (index) {
                            root.updateRule(ruleRow.index, "preset", root.presets[index].id)
                        }
                    }

                    Choice {
                        implicitWidth: 34
                        text: "✕"
                        onClicked: root.removeRule(ruleRow.index)
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: root.rules.length === 0
                text: qsTr("No rules yet.")
                color: "#8b93a1"
                font.pixelSize: 12
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    color: "#5f6773"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    text: backend.busy ? qsTr("Saving…")
                                       : qsTr("Saving writes /etc/tach/profiles.json and asks for your password once.")
                }

                Choice {
                    implicitWidth: 120
                    text: qsTr("Add rule")
                    selected: false
                    onClicked: root.addRule()
                }

                Choice {
                    implicitWidth: 100
                    text: qsTr("Save")
                    selected: true
                    onClicked: backend.saveProfiles(root.autoSwitch, root.fallback, root.rules)
                }
            }
        }
    }
}
