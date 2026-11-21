import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Format.js" as Fmt

Panel {
    id: panel

    property string mode: "auto"
    property var temps: [50, 65, 80, 90]
    property var percents: [0, 35, 60, 100]

    title: qsTr("Fan")
    hint: backend.fanServiceActive ? qsTr("service active") : qsTr("service stopped")

    Component.onCompleted: {
        mode = backend.fanMode
        const curve = backend.fanCurve
        if (curve.length === 4) {
            panel.percents = [
                curve[0].percent, curve[1].percent, curve[2].percent, curve[3].percent
            ]
        }
    }

    function buildCurve() {
        const out = []
        for (let i = 0; i < panel.temps.length; i++)
            out.push({ temp: panel.temps[i], percent: Math.round(panel.percents[i]) })
        return out
    }

    Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: "#8b93a1"
        font.pixelSize: 12
        text: qsTr("Fan control runs in its own system service, not in this window: closing Tach, or a crash, cannot leave the fan pinned. The service writes pwm1 and returns the card to automatic when it stops.")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Repeater {
            model: [
                { id: "auto", label: qsTr("Automatic") },
                { id: "static", label: qsTr("Fixed speed") },
                { id: "curve", label: qsTr("Curve") }
            ]

            delegate: Button {
                required property var modelData

                Layout.fillWidth: true
                highlighted: panel.mode === modelData.id
                text: modelData.label
                onClicked: panel.mode = modelData.id
            }
        }
    }

    // ---- fixed speed ------------------------------------------------
    RowLayout {
        Layout.fillWidth: true
        visible: panel.mode === "static"
        spacing: 12

        Slider {
            id: staticSlider

            Layout.fillWidth: true
            from: 0
            to: 100
            stepSize: 1
            value: backend.fanPercent
        }
        Text {
            Layout.preferredWidth: 48
            horizontalAlignment: Text.AlignRight
            text: Math.round(staticSlider.value) + " %"
            color: "#e8ebef"
            font.pixelSize: 14
            font.bold: true
        }
    }

    // ---- curve ------------------------------------------------------
    ColumnLayout {
        Layout.fillWidth: true
        visible: panel.mode === "curve"
        spacing: 2

        Repeater {
            model: panel.temps

            delegate: RowLayout {
                id: curveRow
                required property int index
                required property var modelData

                Layout.fillWidth: true
                spacing: 12

                Text {
                    Layout.preferredWidth: 52
                    text: curveRow.modelData + " °C"
                    color: "#8b93a1"
                    font.pixelSize: 12
                }

                Slider {
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    stepSize: 1
                    value: panel.percents[curveRow.index]
                    onMoved: {
                        const next = panel.percents.slice()
                        next[curveRow.index] = value
                        panel.percents = next
                    }
                }

                Text {
                    Layout.preferredWidth: 48
                    horizontalAlignment: Text.AlignRight
                    text: Math.round(panel.percents[curveRow.index]) + " %"
                    color: "#e8ebef"
                    font.pixelSize: 13
                }
            }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: panel.mode === "curve"
        wrapMode: Text.WordWrap
        color: "#5f6773"
        font.pixelSize: 11
        text: qsTr("Below 55 °C the fan may stop. Above it Tach never goes under 20 %, whatever the curve says.")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: backend.fanRpm > 0 ? qsTr("now %1 rpm").arg(backend.fanRpm) : qsTr("fan stopped")
            color: "#5f6773"
            font.pixelSize: 11
        }

        Button {
            text: qsTr("Return to automatic")
            enabled: !backend.busy
            onClicked: backend.setFanAuto()
        }
        Button {
            text: qsTr("Apply")
            enabled: !backend.busy && panel.mode !== "auto"
            onClicked: backend.applyFanConfig(panel.mode, Math.round(staticSlider.value), panel.buildCurve())
        }
    }
}
