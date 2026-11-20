.pragma library

// One place for number formatting so every tab reads the same and follows the
// desktop's decimal separator through Qt.locale().
function amount(value, decimals) {
    return Number(value).toLocaleString(Qt.locale(), 'f', decimals)
}

function clock(mhz) {
    return mhz >= 1000 ? amount(mhz / 1000, 2) + " GHz" : mhz + " MHz"
}

function gb(bytes) {
    return amount(bytes / 1073741824, 2) + " GB"
}
