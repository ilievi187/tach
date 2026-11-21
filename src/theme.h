#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTimer>

// Follows the active Omarchy theme: not just the accent, but the background,
// the text, the borders and the warning colours, so a light theme gets a light
// Tach and a dark theme a dark one.
//
// Adapted from Omacut (MIT, David Heinemeier Hansson) for the watching and
// rounding parts.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)
    Q_PROPERTY(bool isLight READ isLight NOTIFY changed)

    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString rail READ rail NOTIFY changed)
    Q_PROPERTY(QString panel READ panel NOTIFY changed)
    Q_PROPERTY(QString panelAlt READ panelAlt NOTIFY changed)
    Q_PROPERTY(QString hover READ hover NOTIFY changed)
    Q_PROPERTY(QString border READ border NOTIFY changed)
    Q_PROPERTY(QString track READ track NOTIFY changed)
    Q_PROPERTY(QString well READ well NOTIFY changed)
    Q_PROPERTY(QString grid READ grid NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QString mutedText READ mutedText NOTIFY changed)
    Q_PROPERTY(QString faintText READ faintText NOTIFY changed)
    Q_PROPERTY(QString danger READ danger NOTIFY changed)
    Q_PROPERTY(QString dangerSoft READ dangerSoft NOTIFY changed)
    Q_PROPERTY(QString dangerBorder READ dangerBorder NOTIFY changed)
    Q_PROPERTY(QString warn READ warn NOTIFY changed)
    Q_PROPERTY(QString info READ info NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString accent() const { return m_accent; }
    QString accentForeground() const;
    int radius() const { return m_radius; }
    bool isLight() const { return m_isLight; }

    QString background() const { return m_background; }
    QString rail() const { return m_rail; }
    QString panel() const { return m_panel; }
    QString panelAlt() const { return m_panelAlt; }
    QString hover() const { return m_hover; }
    QString border() const { return m_border; }
    QString track() const { return m_track; }
    QString well() const { return m_well; }
    QString grid() const { return m_grid; }
    QString foreground() const { return m_foreground; }
    QString mutedText() const { return m_mutedText; }
    QString faintText() const { return m_faintText; }
    QString danger() const { return m_danger; }
    QString dangerSoft() const { return m_dangerSoft; }
    QString dangerBorder() const { return m_dangerBorder; }
    QString warn() const { return m_warn; }
    QString info() const { return m_info; }

    static QString readAccent(const QString &path);
    static QHash<QString, QString> readColors(const QString &path);
    static QString mix(const QString &a, const QString &b, double amount);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();
    void applyPalette(const QHash<QString, QString> &colors, const QString &fallbackAccent);

    QString m_directory;
    QString m_accent = "#FFD60A";
    bool m_isLight = false;
    QString m_background = "#101218";
    QString m_rail = "#0b0d12";
    QString m_panel = "#15181e";
    QString m_panelAlt = "#1a1e25";
    QString m_hover = "#1e222a";
    QString m_border = "#262b33";
    QString m_track = "#1b1f26";
    QString m_well = "#0d0f14";
    QString m_grid = "#191d24";
    QString m_foreground = "#edeff2";
    QString m_mutedText = "#9aa1ab";
    QString m_faintText = "#616873";
    QString m_danger = "#ff6b5e";
    QString m_dangerSoft = "#2a100d";
    QString m_dangerBorder = "#5c2019";
    QString m_warn = "#f2b33d";
    QString m_info = "#7aa2ff";

    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
