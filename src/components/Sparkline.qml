import QtQuick

// A small history line for one metric. Values arrive as the backend's
// QVariantList of ints; the scale is fixed per tile so movement is readable
// against a known ceiling rather than an auto-ranging one that jitters.
Canvas {
    id: root

    property var values: []
    property real ceiling: 100
    property int capacity: 150
    property color lineColor: "#FFD60A"

    onValuesChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onCeilingChanged: requestPaint()
    onCapacityChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)
        if (!values || values.length < 2 || width <= 0 || height <= 0)
            return

        const limit = Math.max(ceiling, 1)
        // Every sample gets a fixed slot, and the newest sits on the right
        // edge, so two seconds of history is a short line at the right rather
        // than a diagonal pretending to be five minutes.
        const slots = Math.max(capacity, values.length)
        const stepX = width / Math.max(slots - 1, 1)
        const firstSlot = slots - values.length
        const xFor = function (index) {
            return (firstSlot + index) * stepX
        }
        const pointY = function (value) {
            return height - Math.min(Math.max(value, 0), limit) / limit * (height - 2) - 1
        }

        ctx.beginPath()
        ctx.moveTo(xFor(0), height)
        for (let i = 0; i < values.length; i++)
            ctx.lineTo(xFor(i), pointY(values[i]))
        ctx.lineTo(xFor(values.length - 1), height)
        ctx.closePath()
        ctx.fillStyle = Qt.rgba(lineColor.r, lineColor.g, lineColor.b, 0.08)
        ctx.fill()

        ctx.beginPath()
        for (let i = 0; i < values.length; i++) {
            const x = xFor(i)
            const y = pointY(values[i])
            if (i === 0)
                ctx.moveTo(x, y)
            else
                ctx.lineTo(x, y)
        }
        ctx.strokeStyle = lineColor
        ctx.lineWidth = 1.5
        ctx.lineJoin = "round"
        ctx.stroke()
    }
}
