// Theme watching follows Omacut (MIT, David Heinemeier Hansson).
#include "theme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <cmath>

Theme::Theme(const QString &directory, QObject *parent, const QString &hyprctl)
    : QObject(parent),
      m_directory(!qEnvironmentVariable("TACH_THEME_DIR").isEmpty()
                      ? qEnvironmentVariable("TACH_THEME_DIR")
                      : directory.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current"
                                            : directory) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);

    // Corners follow Hyprland's effective rounding, which themes and personal
    // overrides can both set, so ask hyprctl rather than reading its config.
    // Without Hyprland the corners stay sharp, like Omarchy's default.
    m_hyprctl = !hyprctl.isEmpty() ? hyprctl
              : directory.isEmpty() && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")
                  ? QStandardPaths::findExecutable("hyprctl") : QString();
    m_roundingTimeout.setSingleShot(true);
    m_roundingTimeout.setInterval(500);
    connect(&m_roundingTimeout, &QTimer::timeout, &m_roundingProcess, &QProcess::kill);
    connect(&m_roundingProcess, &QProcess::errorOccurred, this, [this] { m_roundingTimeout.stop(); });
    connect(&m_roundingProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_roundingTimeout.stop();
        const auto value = QJsonDocument::fromJson(m_roundingProcess.readAllStandardOutput()).object().value("int");
        if (code != 0 || status != QProcess::NormalExit || !value.isDouble() || value.toInt(-1) < 0)
            return;
        if (value.toInt() != m_radius) {
            m_radius = value.toInt();
            emit changed();
        }
    });
    m_roundingPoll.setInterval(1000);
    connect(&m_roundingPoll, &QTimer::timeout, this, &Theme::refreshRounding);
    if (!m_hyprctl.isEmpty())
        m_roundingPoll.start();
    reload();
}

Theme::~Theme() {
    if (m_roundingProcess.state() != QProcess::NotRunning) {
        m_roundingProcess.kill();
        m_roundingProcess.waitForFinished(1000);
    }
}

void Theme::refreshRounding() {
    if (m_hyprctl.isEmpty() || m_roundingProcess.state() != QProcess::NotRunning)
        return;
    m_roundingProcess.start(m_hyprctl, {"-j", "getoption", "decoration:rounding"});
    m_roundingTimeout.start();
}

// Every theme file is a flat list of key = "#rrggbb"; parse all of it so the
// window, panels and warnings can follow too, not just the accent.
QHash<QString, QString> Theme::readColors(const QString &path) {
    QHash<QString, QString> colors;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return colors;

    static const QRegularExpression entry(
        R"re(^\s*([A-Za-z_]+)\s*=\s*["']?(#[0-9a-fA-F]{6})["']?)re",
        QRegularExpression::MultilineOption);
    const QString text = QString::fromUtf8(file.readAll());
    auto it = entry.globalMatch(text);
    while (it.hasNext()) {
        const auto match = it.next();
        colors.insert(match.captured(1).toLower(), match.captured(2));
    }

    static const QRegularExpression mode(R"re(^\s*mode\s*=\s*["']?(\w+))re",
                                         QRegularExpression::MultilineOption);
    const auto modeMatch = mode.match(text);
    if (modeMatch.hasMatch())
        colors.insert("mode", modeMatch.captured(1).toLower());
    return colors;
}

QString Theme::readAccent(const QString &path) {
    const auto colors = readColors(path);
    return colors.value("accent", "#FFD60A");
}

// Straight sRGB blend; good enough for tints and borders and predictable
// across both light and dark themes.
QString Theme::mix(const QString &a, const QString &b, double amount) {
    const QColor first(a);
    const QColor second(b);
    if (!first.isValid() || !second.isValid())
        return a;
    const double t = qBound(0.0, amount, 1.0);
    auto blend = [t](int x, int y) { return int(std::round(x + (y - x) * t)); };
    return QColor(blend(first.red(), second.red()),
                  blend(first.green(), second.green()),
                  blend(first.blue(), second.blue()))
        .name();
}

namespace {
double colourDistance(const QString &a, const QString &b) {
    const QColor first(a), second(b);
    if (!first.isValid() || !second.isValid())
        return 0;
    const double dr = first.red() - second.red();
    const double dg = first.green() - second.green();
    const double db = first.blue() - second.blue();
    return std::sqrt(dr * dr + dg * dg + db * db);
}

// Themes are free to make, say, blue the accent — in which case the "info"
// line on the chart would be the same colour as the load line. Walk the
// candidates and take the first that is clearly different from the accent.
QString pickDistinct(const QHash<QString, QString> &colors,
                     std::initializer_list<const char *> candidates,
                     const QString &accent, const QString &fallback) {
    QString chosen = fallback;
    for (const char *key : candidates) {
        const QString value = colors.value(QLatin1String(key));
        if (!value.isEmpty()) {
            chosen = value;
            break;
        }
    }
    for (const char *key : candidates) {
        const QString value = colors.value(QLatin1String(key));
        if (!value.isEmpty() && colourDistance(value, accent) > 90)
            return value;
    }

    const QColor base(accent);
    if (base.isValid()) {
        float hue = 0, saturation = 0, value = 0, alpha = 0;
        base.getHsvF(&hue, &saturation, &value, &alpha);
        if (hue < 0) {
            hue = 0.55f;             // grey accent: start from blue
            saturation = 0.45f;
        }
        return QColor::fromHsvF(std::fmod(hue + 0.42f, 1.0f),
                                std::max(saturation, 0.45f),
                                std::max(value, 0.65f))
            .name();
    }
    return chosen;
}
} // namespace

void Theme::applyPalette(const QHash<QString, QString> &colors, const QString &fallbackAccent) {
    const QString background = colors.value("background", "#101218");
    const QString foreground = colors.value("foreground", "#edeff2");
    const QString accent = colors.value("accent", fallbackAccent);

    const QColor bg(background);
    const bool light = colors.value("mode") == QLatin1String("light") ||
                       (bg.isValid() && bg.lightnessF() > 0.55);

    m_isLight = light;
    m_accent = accent;

    m_background = background;
    m_rail = colors.value("dark_background",
                          mix(background, light ? "#ffffff" : "#000000", 0.18));
    m_panel = mix(background, foreground, light ? 0.035 : 0.045);
    m_panelAlt = mix(background, foreground, light ? 0.07 : 0.09);
    m_hover = colors.value("lighter_background", mix(background, foreground, 0.12));
    m_border = mix(background, foreground, 0.15);
    m_track = mix(background, foreground, 0.08);
    m_well = colors.value("darker_background", mix(background, light ? "#ffffff" : "#000000", 0.10));
    m_grid = mix(background, foreground, 0.075);

    m_foreground = foreground;
    m_mutedText = mix(foreground, background, 0.42);
    m_faintText = mix(foreground, background, 0.62);

    m_danger = pickDistinct(colors, {"red", "bright_red"}, accent, "#ff6b5e");
    m_dangerSoft = mix(background, m_danger, 0.16);
    m_dangerBorder = mix(background, m_danger, 0.38);
    m_warn = pickDistinct(colors, {"orange", "yellow", "bright_yellow"}, accent, "#f2b33d");
    m_info = pickDistinct(colors, {"blue", "cyan", "magenta", "bright_blue", "bright_cyan"},
                          accent, "#7aa2ff");
}

// Black or white, whichever reads on the accent (WCAG relative luminance).
QString Theme::accentForeground() const {
    const QColor c(m_accent);
    auto linear = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
    const double luminance = .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
    return luminance > .179 ? "black" : "white";
}

void Theme::reload() {
    refreshRounding();
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    // A theme switch replaces symlinks and files, so watch the parents too and
    // re-arm after every change. Watching the nearest existing ancestor means
    // installing Omarchy later works without restarting the app.
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory,
                           m_directory + "/theme", m_directory + "/theme/colors.toml"};
    candidates.removeDuplicates();
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    const auto colors = readColors(m_directory + "/theme/colors.toml");
    const QString previous = m_background + m_accent + m_foreground;
    applyPalette(colors, "#FFD60A");
    if (previous != m_background + m_accent + m_foreground)
        emit changed();
}
