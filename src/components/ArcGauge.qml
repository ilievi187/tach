import QtQuick

// A 270° ring for one value. The value and its unit are drawn as separate
// Text items so they stay crisp rather than being painted into the canvas.
Item {
    id: root

    property real value: 0          // 0..100
    property string unit: "%"
    property color accentColor: theme.accent
    property color trackColor: theme.track
    property int thickness: 8

    onValueChanged: gauge.requestPaint()
    onAccentColorChanged: gauge.requestPaint()
    onTrackColorChanged: gauge.requestPaint()
    onWidthChanged: gauge.requestPaint()
    onHeightChanged: gauge.requestPaint()

    Canvas {
        id: gauge
        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            const radius = Math.min(width, height) / 2 - root.thickness / 2 - 2
            const cx = width / 2
            const cy = height / 2
            const start = Math.PI * 0.75
            const sweep = Math.PI * 1.5
            const clamped = Math.max(0, Math.min(root.value, 100))

            ctx.lineCap = "round"
            ctx.lineWidth = root.thickness

            ctx.beginPath()
            ctx.arc(cx, cy, radius, start, start + sweep)
            ctx.strokeStyle = root.trackColor
            ctx.stroke()

            if (clamped > 0) {
                ctx.beginPath()
                ctx.arc(cx, cy, radius, start, start + sweep * clamped / 100)
                ctx.lineWidth = root.thickness
                ctx.strokeStyle = root.accentColor
                ctx.stroke()
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: -2

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: Math.round(root.value)
            color: theme.foreground
            font.pixelSize: Math.max(22, Math.round(root.height * 0.26))
            font.bold: true
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.unit
            color: theme.mutedText
            font.pixelSize: Math.max(10, Math.round(root.height * 0.09))
        }
    }
}
