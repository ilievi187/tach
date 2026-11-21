import QtQuick

// Drag the four points: x is the temperature, y is the fan percentage. The
// curve between them is the one the service actually interpolates, and the
// vertical line is where the card is right now.
Item {
    id: root

    property var points: []
    property int currentTemp: 0
    property color accentColor: theme.accent
    signal edited(var updated)

    implicitWidth: 320
    implicitHeight: 200

    readonly property real padLeft: 32
    readonly property real padRight: 12
    readonly property real padTop: 12
    readonly property real padBottom: 22

    function plotWidth() { return Math.max(1, width - padLeft - padRight) }
    function plotHeight() { return Math.max(1, height - padTop - padBottom) }
    function xFor(temp) { return padLeft + (Math.min(Math.max(temp, 30), 100) - 30) / 70 * plotWidth() }
    function yFor(percent) { return padTop + (1 - Math.min(Math.max(percent, 0), 100) / 100) * plotHeight() }
    function tempFor(x) { return 30 + (x - padLeft) / plotWidth() * 70 }
    function percentFor(y) { return (1 - (y - padTop) / plotHeight()) * 100 }

    function sorted() {
        return points.slice().sort(function (a, b) { return a.temp - b.temp })
    }

    function valueAt(temp) {
        const list = sorted()
        if (list.length === 0)
            return 0
        if (temp <= list[0].temp)
            return list[0].percent
        if (temp >= list[list.length - 1].temp)
            return list[list.length - 1].percent
        for (let i = 1; i < list.length; i++) {
            if (temp <= list[i].temp) {
                const a = list[i - 1], b = list[i]
                const span = b.temp - a.temp
                if (span <= 0)
                    return b.percent
                return Math.round(a.percent + (temp - a.temp) / span * (b.percent - a.percent))
            }
        }
        return list[list.length - 1].percent
    }

    onPointsChanged: canvas.requestPaint()
    onCurrentTempChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (width <= 0 || height <= 0)
                return

            const left = root.padLeft, top = root.padTop
            const w = root.plotWidth(), h = root.plotHeight()

            // plot background
            ctx.fillStyle = "#0f1116"
            ctx.fillRect(left, top, w, h)
            ctx.strokeStyle = "#1d2028"
            ctx.lineWidth = 1
            ctx.strokeRect(left + 0.5, top + 0.5, w - 1, h - 1)

            // horizontal guides
            ctx.strokeStyle = "#191c23"
            for (let i = 1; i < 4; i++) {
                const y = top + h * i / 4
                ctx.beginPath()
                ctx.moveTo(left, y)
                ctx.lineTo(left + w, y)
                ctx.stroke()
            }

            // axis labels
            ctx.fillStyle = "#5f6773"
            ctx.font = "10px sans-serif"
            ctx.textAlign = "right"
            ctx.textBaseline = "middle"
            ctx.fillText("100", left - 6, top + 4)
            ctx.fillText("50", left - 6, top + h / 2)
            ctx.fillText("0", left - 6, top + h - 4)

            ctx.textAlign = "center"
            ctx.textBaseline = "top"
            ctx.fillText("30", left, top + h + 5)
            ctx.fillText("65", left + w / 2, top + h + 5)
            ctx.fillText("100 °C", left + w, top + h + 5)

            const list = root.sorted()
            if (list.length < 2)
                return

            // fill under the curve
            ctx.beginPath()
            ctx.moveTo(root.xFor(list[0].temp), top + h)
            for (const point of list)
                ctx.lineTo(root.xFor(point.temp), root.yFor(point.percent))
            ctx.lineTo(root.xFor(list[list.length - 1].temp), top + h)
            ctx.closePath()
            ctx.fillStyle = Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.10)
            ctx.fill()

            // the curve
            ctx.beginPath()
            for (let i = 0; i < list.length; i++) {
                const x = root.xFor(list[i].temp)
                const y = root.yFor(list[i].percent)
                if (i === 0)
                    ctx.moveTo(x, y)
                else
                    ctx.lineTo(x, y)
            }
            ctx.strokeStyle = root.accentColor
            ctx.lineWidth = 2
            ctx.lineJoin = "round"
            ctx.stroke()

            // handles
            for (const point of list) {
                const x = root.xFor(point.temp)
                const y = root.yFor(point.percent)
                ctx.beginPath()
                ctx.arc(x, y, 5.5, 0, Math.PI * 2)
                ctx.fillStyle = "#0f1116"
                ctx.fill()
                ctx.beginPath()
                ctx.arc(x, y, 4, 0, Math.PI * 2)
                ctx.fillStyle = root.accentColor
                ctx.fill()
            }

            // where the card is right now
            if (root.currentTemp > 25) {
                const x = root.xFor(root.currentTemp)
                ctx.save()
                ctx.setLineDash([3, 3])
                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.28)
                ctx.beginPath()
                ctx.moveTo(x, top)
                ctx.lineTo(x, top + h)
                ctx.stroke()
                ctx.restore()

                const percent = root.valueAt(root.currentTemp)
                ctx.beginPath()
                ctx.arc(x, root.yFor(percent), 3, 0, Math.PI * 2)
                ctx.fillStyle = "#ffffff"
                ctx.fill()

                // A fixed corner keeps the readout off the curve and off the
                // right edge when the card is hot.
                const label = root.currentTemp + " °C → " + percent + " %"
                ctx.fillStyle = "rgba(10, 11, 14, 0.85)"
                ctx.fillRect(left + 4, top + 4, 92, 16)
                ctx.fillStyle = "#ffffff"
                ctx.textAlign = "left"
                ctx.textBaseline = "middle"
                ctx.fillText(label, left + 9, top + 12)
            }
        }
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent

        property int dragged: -1

        function nearestIndex(x, y) {
            let best = -1
            let bestDistance = 26 * 26
            for (let i = 0; i < root.points.length; i++) {
                const dx = root.xFor(root.points[i].temp) - x
                const dy = root.yFor(root.points[i].percent) - y
                const distance = dx * dx + dy * dy
                if (distance < bestDistance) {
                    bestDistance = distance
                    best = i
                }
            }
            return best
        }

        onPressed: function (mouse) {
            dragged = nearestIndex(mouse.x, mouse.y)
        }

        onPositionChanged: function (mouse) {
            if (dragged < 0)
                return

            const list = root.sorted()
            const moving = list.findIndex(function (p) {
                return p.temp === root.points[dragged].temp && p.percent === root.points[dragged].percent
            })
            const index = moving >= 0 ? moving : dragged

            // Keep the points in temperature order with a small gap, so the
            // curve can never fold back on itself.
            const lower = index > 0 ? list[index - 1].temp + 2 : 30
            const upper = index < list.length - 1 ? list[index + 1].temp - 2 : 100
            const temp = Math.round(Math.min(Math.max(root.tempFor(mouse.x), lower), upper))
            const percent = Math.round(Math.min(Math.max(root.percentFor(mouse.y), 0), 100))

            const updated = list.slice()
            updated[index] = { temp: temp, percent: percent }
            root.edited(updated)
        }

        onReleased: dragged = -1
    }
}
