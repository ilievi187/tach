#pragma once

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

// Reads an AMD GPU out of sysfs and keeps a short history of it.
//
// Readings need no privileges. Controls do: the DPM performance level, the
// power profile mode and the power cap are root-only sysfs files, so every
// write goes through pkexec (`tach set-…`) instead of being attempted here.
//
// Overdrive (clocks, voltage, fan curve) is only present when the kernel was
// booted with amdgpu.ppfeaturemask=0xffffffff; when the files are missing the
// UI says so rather than offering controls that cannot work.
//
// The sysfs root is a constructor argument so tests can point it at a fake tree.
class Backend : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool available READ available NOTIFY cardChanged)
    Q_PROPERTY(QString cardName READ cardName NOTIFY cardChanged)
    Q_PROPERTY(QString cardPath READ cardPath NOTIFY cardChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

    Q_PROPERTY(double utilisation READ utilisation NOTIFY sampleChanged)
    Q_PROPERTY(double memUtilisation READ memUtilisation NOTIFY sampleChanged)
    Q_PROPERTY(double vramUsedGb READ vramUsedGb NOTIFY sampleChanged)
    Q_PROPERTY(double vramTotalGb READ vramTotalGb NOTIFY sampleChanged)
    Q_PROPERTY(double vramFraction READ vramFraction NOTIFY sampleChanged)
    Q_PROPERTY(int edgeTemp READ edgeTemp NOTIFY sampleChanged)
    Q_PROPERTY(int junctionTemp READ junctionTemp NOTIFY sampleChanged)
    Q_PROPERTY(int memTemp READ memTemp NOTIFY sampleChanged)
    Q_PROPERTY(double power READ power NOTIFY sampleChanged)
    Q_PROPERTY(int fanRpm READ fanRpm NOTIFY sampleChanged)
    Q_PROPERTY(int fanMax READ fanMax NOTIFY sampleChanged)
    Q_PROPERTY(int coreClock READ coreClock NOTIFY sampleChanged)
    Q_PROPERTY(int memClock READ memClock NOTIFY sampleChanged)
    Q_PROPERTY(QString pcie READ pcie NOTIFY sampleChanged)
    Q_PROPERTY(QString perfLevel READ perfLevel NOTIFY sampleChanged)
    Q_PROPERTY(QVariantList utilHistory READ utilHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList tempHistory READ tempHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList powerHistory READ powerHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList processes READ processes NOTIFY processesChanged)

    Q_PROPERTY(QString powerProfile READ powerProfile NOTIFY sampleChanged)
    Q_PROPERTY(int powerProfileIndex READ powerProfileIndex NOTIFY sampleChanged)
    Q_PROPERTY(QVariantList powerProfiles READ powerProfiles NOTIFY sampleChanged)
    Q_PROPERTY(double powerCapW READ powerCapW NOTIFY sampleChanged)
    Q_PROPERTY(double powerCapMinW READ powerCapMinW NOTIFY sampleChanged)
    Q_PROPERTY(double powerCapMaxW READ powerCapMaxW NOTIFY sampleChanged)
    Q_PROPERTY(double powerCapDefaultW READ powerCapDefaultW NOTIFY sampleChanged)

    Q_PROPERTY(bool canControl READ canControl NOTIFY cardChanged)
    Q_PROPERTY(bool canPowerCap READ canPowerCap NOTIFY cardChanged)
    Q_PROPERTY(bool canOverdrive READ canOverdrive NOTIFY cardChanged)
    Q_PROPERTY(bool canFanControl READ canFanControl NOTIFY cardChanged)
    Q_PROPERTY(QString overdriveHint READ overdriveHint NOTIFY cardChanged)
    Q_PROPERTY(QVariantMap overlayTools READ overlayTools NOTIFY cardChanged)

    Q_PROPERTY(QString fanMode READ fanMode NOTIFY fanChanged)
    Q_PROPERTY(int fanPercent READ fanPercent NOTIFY fanChanged)
    Q_PROPERTY(QVariantList fanCurve READ fanCurve NOTIFY fanChanged)
    Q_PROPERTY(bool fanServiceActive READ fanServiceActive NOTIFY fanChanged)
    Q_PROPERTY(bool powerDaemonActive READ powerDaemonActive NOTIFY cardChanged)
    Q_PROPERTY(bool dpmManagedByTach READ dpmManagedByTach NOTIFY cardChanged)

    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)

public:
    explicit Backend(const QString &drmRoot = {}, QObject *parent = nullptr);

    bool available() const { return !m_card.isEmpty(); }
    QString cardName() const { return m_cardName; }
    QString cardPath() const { return m_card; }
    QString error() const { return m_error; }

    double utilisation() const { return m_utilisation; }
    double memUtilisation() const { return m_memUtilisation; }
    double vramUsedGb() const { return m_vramUsed / 1073741824.0; }
    double vramTotalGb() const { return m_vramTotal / 1073741824.0; }
    double vramFraction() const { return m_vramTotal > 0 ? double(m_vramUsed) / double(m_vramTotal) : 0.0; }
    int edgeTemp() const { return m_edgeTemp; }
    int junctionTemp() const { return m_junctionTemp; }
    int memTemp() const { return m_memTemp; }
    double power() const { return m_power; }
    int fanRpm() const { return m_fanRpm; }
    int fanMax() const { return m_fanMax; }
    int coreClock() const { return m_coreClock; }
    int memClock() const { return m_memClock; }
    QString pcie() const { return m_pcie; }
    QString perfLevel() const { return m_perfLevel; }

    QVariantList utilHistory() const { return m_utilHistory; }
    QVariantList tempHistory() const { return m_tempHistory; }
    QVariantList powerHistory() const { return m_powerHistory; }
    QVariantList processes() const { return m_processes; }

    QString powerProfile() const { return m_powerProfileName; }
    int powerProfileIndex() const { return m_powerProfileIndex; }
    QVariantList powerProfiles() const { return m_powerProfiles; }
    double powerCapW() const { return m_powerCapW; }
    double powerCapMinW() const { return m_powerCapMinW; }
    double powerCapMaxW() const { return m_powerCapMaxW; }
    double powerCapDefaultW() const { return m_powerCapDefaultW; }

    bool canControl() const { return !m_controlFile.isEmpty(); }
    bool canPowerCap() const { return !m_powerCapFile.isEmpty(); }
    bool canOverdrive() const { return m_canOverdrive; }
    bool canFanControl() const { return m_canFanControl; }
    QString overdriveHint() const;
    QVariantMap overlayTools() const { return m_overlayTools; }

    QString fanMode() const { return m_fanMode; }
    int fanPercent() const { return m_fanPercent; }
    QVariantList fanCurve() const { return m_fanCurve; }
    bool fanServiceActive() const { return m_fanServiceActive; }
    bool powerDaemonActive() const { return m_powerDaemonActive; }
    bool dpmManagedByTach() const { return m_dpmManagedByTach; }

    QString status() const { return m_status; }
    bool busy() const { return m_busy; }

    // Reads one sample and appends to the history. Public so the CLI and the
    // tests can drive it without an event loop.
    void poll();
    Q_INVOKABLE void refresh() { poll(); }

    Q_INVOKABLE void setPerformanceLevel(const QString &level);
    Q_INVOKABLE void setPowerProfile(int index);
    Q_INVOKABLE void setPowerCap(double watts);
    Q_INVOKABLE void setCardPath(const QString &cardPath);

    // Overlay configuration for tools that are driven by files rather than
    // by the driver. Each write keeps a backup and only replaces our own
    // marked block, so a hand-written config survives.
    Q_INVOKABLE QString writeMangoHudPreset(const QString &preset);
    Q_INVOKABLE QString writeVkBasaltPreset(const QString &preset);
    Q_INVOKABLE QString launchOptions(const QString &tool);
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE void openUrl(const QString &url);

    // Fan control runs as a root service (tach-fan.service) so the fan never
    // stays pinned if the window closes or crashes: the GUI only writes a
    // config and lets the service apply it.
    Q_INVOKABLE void applyFanConfig(const QString &mode, int percent, const QVariantList &curve);
    Q_INVOKABLE void setFanAuto();
    Q_INVOKABLE void takeOverDpm();
    Q_INVOKABLE void restoreDpm();

    static int curvePercentAt(const QVariantList &curve, int temperature);
    static QVariantList defaultFanCurve();
    static QString fanConfigPath();
    QString hwmonPath() const;

    // --- parsing helpers, unit tested directly -------------------------
    static int currentClockMhz(const QString &table);
    static qint64 parseSize(const QString &text);
    static QString cardNameForDeviceId(quint32 deviceId);
    static qint64 vramForProcess(const QString &fdinfo, const QString &pciAddress);
    static QStringList performanceLevels();
    static QVariantList parsePowerProfiles(const QString &table, int *activeIndex);
    static QString mergeManagedBlock(const QString &existing, const QString &block);
    static QHash<QString, qint64> engineTimeForProcess(const QString &fdinfo, const QString &pciAddress);

    static constexpr int HistoryLength = 60;

signals:
    void cardChanged();
    void sampleChanged();
    void historyChanged();
    void processesChanged();
    void statusChanged();
    void errorChanged();
    void fanChanged();

private:
    void discoverCard();
    void setStatus(const QString &message, bool busy = false);
    void refreshProcesses();
    void appendHistory(QVariantList &list, int value);
    void detectOverlayTools();
    void refreshFanState();
    void readFanConfig();
    void runPrivileged(const QStringList &arguments, const QString &successMessage);

    QString readFile(const QString &path) const;
    bool writeRootFile(const QString &path, const QString &contents, QString *failure);

    QString m_root;
    QString m_card;
    QString m_cardName;
    QString m_pci;
    QString m_controlFile;
    QString m_powerCapFile;
    QString m_profileFile;
    QString m_error;
    bool m_canOverdrive = false;
    bool m_canFanControl = false;
    QVariantMap m_overlayTools;

    double m_utilisation = 0;
    double m_memUtilisation = 0;
    qint64 m_vramUsed = 0;
    qint64 m_vramTotal = 0;
    int m_edgeTemp = 0;
    int m_junctionTemp = 0;
    int m_memTemp = 0;
    double m_power = 0;
    int m_fanRpm = 0;
    int m_fanMax = 0;
    int m_coreClock = 0;
    int m_memClock = 0;
    QString m_pcie;
    QString m_perfLevel;

    QString m_powerProfileName;
    int m_powerProfileIndex = -1;
    QVariantList m_powerProfiles;
    double m_powerCapW = 0;
    double m_powerCapMinW = 0;
    double m_powerCapMaxW = 0;
    double m_powerCapDefaultW = 0;

    QVariantList m_utilHistory;
    QVariantList m_tempHistory;
    QVariantList m_powerHistory;
    QVariantList m_processes;

    // Previous engine counters, for turning cumulative nanoseconds into a
    // per-process percentage between polls.
    QHash<int, qint64> m_engineTotals;
    qint64 m_engineStampNs = 0;

    QString m_fanMode = "auto";
    int m_fanPercent = 50;
    QVariantList m_fanCurve;
    bool m_fanServiceActive = false;
    bool m_powerDaemonActive = false;
    bool m_dpmManagedByTach = false;
    int m_fanTicks = 0;

    QString m_status;
    bool m_busy = false;
    int m_ticks = 0;

    QTimer m_timer;
};
