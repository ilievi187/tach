#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>

// Reads an AMD GPU out of sysfs and keeps a short history of it.
//
// Every value here is readable without privileges. The one control AMD exposes
// on this driver is the DPM performance level, and that needs root, so it goes
// through pkexec (`tach set-level ...`) instead of writing the file directly.
//
// The sysfs root is a constructor argument so tests can point it at a fake tree.
class Backend : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool available READ available NOTIFY cardChanged)
    Q_PROPERTY(QString cardName READ cardName NOTIFY cardChanged)
    Q_PROPERTY(QString cardPath READ cardPath NOTIFY cardChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool canControl READ canControl NOTIFY cardChanged)

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

    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)

public:
    explicit Backend(const QString &drmRoot = {}, QObject *parent = nullptr);

    bool available() const { return !m_card.isEmpty(); }
    QString cardName() const { return m_cardName; }
    QString cardPath() const { return m_card; }
    QString error() const { return m_error; }
    bool canControl() const { return !m_controlFile.isEmpty(); }

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

    QString status() const { return m_status; }
    bool busy() const { return m_busy; }

    // Reads one sample and appends to the history. Public so the CLI and the
    // tests can drive it without an event loop.
    void poll();
    Q_INVOKABLE void refresh() { poll(); }

    Q_INVOKABLE void setPerformanceLevel(const QString &level);
    Q_INVOKABLE void setCardPath(const QString &cardPath);

    // --- parsing helpers, unit tested directly -------------------------
    static int currentClockMhz(const QString &table);
    static qint64 parseSize(const QString &text);
    static QString cardNameForDeviceId(quint32 deviceId);
    static qint64 vramForProcess(const QString &fdinfo, const QString &pciAddress);
    static QStringList performanceLevels();

    static constexpr int HistoryLength = 60;

signals:
    void cardChanged();
    void sampleChanged();
    void historyChanged();
    void processesChanged();
    void statusChanged();
    void errorChanged();

private:
    void discoverCard();
    void setStatus(const QString &message, bool busy = false);
    void refreshProcesses();
    void appendHistory(QVariantList &list, int value);

    QString readFile(const QString &path) const;

    QString m_root;
    QString m_card;
    QString m_cardName;
    QString m_pci;
    QString m_controlFile;
    QString m_error;

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

    QVariantList m_utilHistory;
    QVariantList m_tempHistory;
    QVariantList m_powerHistory;
    QVariantList m_processes;

    QString m_status;
    bool m_busy = false;
    int m_ticks = 0;

    QTimer m_timer;
};
