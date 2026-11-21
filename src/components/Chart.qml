import QtQuick

// Three metrics on one shared timeline. Each series keeps its own ceiling so a
// 300 W spike and a 100 % load are both legible, and all three share the same
// slot width so the x axis means the same thing for everyone.
Canvas {
    id: root

    property var utilisation: []
    property var temperature: []
    property var power: []
    property color utilisationColor: theme.accent
    property color temperatureColor: theme.warn
    property color powerColor: theme.info
    property int capacity: 60
    property int hoverSlot: -1

    readonly property int points: Math.max(utilisation.length, temperature.length, power.length)

    onUtilisationChanged: requestPaint()
    onTemperatureChanged: requestPaint()
    onPowerChanged: requestPaint()
    // A theme switch changes the colours without touching the data.
    onUtilisationColorChanged: requestPaint()
    onTemperatureColorChanged: requestPaint()
    onPowerColorChanged: requestPaint()
    onCapacityChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    function drawSeries(ctx, values, ceiling, colour, slots, firstSlot, stepX) {
        if (!values || values.length < 2)
            return
        ctx.beginPath()
        for (let i = 0; i < values.length; i++) {
            const x = (firstSlot + i) * stepX
            const y = height - Math.min(Math.max(values[i], 0), ceiling) / ceiling * (height - 8) - 4
            if (i === 0)
                ctx.moveTo(x, y)
            else
                ctx.lineTo(x, y)
        }
        ctx.strokeStyle = colour
        ctx.lineWidth = 1.6
        ctx.lineJoin = "round"
        ctx.stroke()
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true

        function slotAt(x) {
            const slots = Math.max(root.capacity, root.points)
            const stepX = width / Math.max(slots - 1, 1)
            return Math.round(x / Math.max(stepX, 0.001))
        }

        onPositionChanged: function (mouse) {
            const slot = slotAt(mouse.x)
            if (slot !== root.hoverSlot) {
                root.hoverSlot = slot
                root.requestPaint()
            }
        }
        onExited: {
            root.hoverSlot = -1
            root.requestPaint()
        }
    }

    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        if (width <= 0 || height <= 0)
            return

        // Horizontal guides at 25/50/75 % of the height, so movement has a
        // reference without drawing axes nobody reads.
        ctx.strokeStyle = theme.track
        ctx.lineWidth = 1
        for (let i = 1; i < 4; i++) {
            const y = height * i / 4
            ctx.beginPath()
            ctx.moveTo(0, y)
            ctx.lineTo(width, y)
            ctx.stroke()
        }

        const slots = Math.max(capacity, points)
        const stepX = width / Math.max(slots - 1, 1)
        const firstSlot = slots - points

        drawSeries(ctx, power, 400, powerColor, slots, firstSlot, stepX)
        drawSeries(ctx, temperature, 110, temperatureColor, slots, firstSlot, stepX)
        drawSeries(ctx, utilisation, 100, utilisationColor, slots, firstSlot, stepX)

        // Hover readout: the sample under the pointer, so a spike can be
        // measured instead of guessed from the shape.
        const index = root.hoverSlot - firstSlot
        const hasSample = root.hoverSlot >= 0 && index >= 0 && index < root.points
        if (hasSample) {
            const x = root.hoverSlot * stepX
            ctx.save()
            ctx.setLineDash([3, 3])
            ctx.strokeStyle = "rgba(255, 255, 255, 0.30)"
            ctx.beginPath()
            ctx.moveTo(x, 0)
            ctx.lineTo(x, height)
            ctx.stroke()
            ctx.restore()

            const parts = []
            if (index < utilisation.length) parts.push(Math.round(utilisation[index]) + " %")
            if (index < temperature.length) parts.push(Math.round(temperature[index]) + " °C")
            if (index < power.length) parts.push(Math.round(power[index]) + " W")

            for (const pair of [[utilisation, utilisationColor], [temperature, temperatureColor], [power, powerColor]]) {
                const values = pair[0]
                if (index >= values.length) continue
                const ceiling = values === utilisation ? 100 : values === temperature ? 110 : 400
                const y = height - Math.min(Math.max(values[index], 0), ceiling) / ceiling * (height - 8) - 4
                ctx.beginPath()
                ctx.arc(x, y, 2.5, 0, Math.PI * 2)
                ctx.fillStyle = pair[1]
                ctx.fill()
            }

            const label = parts.join("  ·  ")
            ctx.font = "11px sans-serif"
            ctx.textAlign = "left"
            ctx.textBaseline = "middle"
            const labelWidth = ctx.measureText(label).width + 14
            const labelX = Math.min(Math.max(x + 10, 4), width - labelWidth - 4)
            ctx.fillStyle = "rgba(8, 9, 12, 0.85)"
            ctx.fillRect(labelX, 6, labelWidth, 20)
            ctx.fillStyle = "#ffffff"
            ctx.fillText(label, labelX + 7, 16)
        }
    }
}
