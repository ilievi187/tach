import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Format.js" as Fmt

Panel {
    id: panel

    property string mode: "auto"
    property var points: [
        { temp: 50, percent: 0 },
        { temp: 65, percent: 35 },
        { temp: 80, percent: 60 },
        { temp: 90, percent: 100 }
    ]

    title: qsTr("Fan")
    hint: backend.fanServiceActive ? qsTr("service active") : qsTr("service stopped")

    Component.onCompleted: {
        mode = backend.fanMode
        if (backend.fanCurve.length >= 2)
            panel.points = backend.fanCurve
    }

    Text {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: theme.mutedText
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

            delegate: Choice {
                required property var modelData

                Layout.fillWidth: true
                selected: panel.mode === modelData.id
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
            color: theme.foreground
            font.pixelSize: 14
            font.bold: true
        }
    }

    // ---- curve ------------------------------------------------------
    CurveEditor {
        Layout.fillWidth: true
        Layout.preferredHeight: 196
        visible: panel.mode === "curve"
        points: panel.points
        currentTemp: backend.junctionTemp
        onEdited: function (updated) { panel.points = updated }
    }

    Text {
        Layout.fillWidth: true
        visible: panel.mode === "curve"
        wrapMode: Text.WordWrap
        color: theme.faintText
        font.pixelSize: 11
        text: qsTr("Below 55 °C the fan may stop. Above it Tach never goes under 20 %, whatever the curve says.")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Text {
            Layout.fillWidth: true
            text: backend.fanRpm > 0 ? qsTr("now %1 rpm").arg(backend.fanRpm) : qsTr("fan stopped")
            color: theme.faintText
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
            onClicked: backend.applyFanConfig(panel.mode, Math.round(staticSlider.value), panel.points)
        }
    }
}
