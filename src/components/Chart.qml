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
    property color temperatureColor: "#ff8f6b"
    property color powerColor: "#7aa2ff"
    property int capacity: 60

    readonly property int points: Math.max(utilisation.length, temperature.length, power.length)

    onUtilisationChanged: requestPaint()
    onTemperatureChanged: requestPaint()
    onPowerChanged: requestPaint()
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

    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        if (width <= 0 || height <= 0)
            return

        // Horizontal guides at 25/50/75 % of the height, so movement has a
        // reference without drawing axes nobody reads.
        ctx.strokeStyle = "#1b1e26"
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
    }
}
