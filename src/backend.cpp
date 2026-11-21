#include "backend.h"

#include <QClipboard>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

#include "profiles.h"
#include <QStringList>

namespace {
const QStringList kLevels{"auto", "high", "low"};
const QString kBlockBegin = "# >>> tach managed block >>>";
const QString kBlockEnd = "# <<< tach managed block <<<";

QString firstHwmon(const QString &card) {
    const QDir dir(card + "/hwmon");
    const auto entries = dir.entryList({"hwmon*"}, QDir::Dirs | QDir::NoDotAndDotDot);
    return entries.isEmpty() ? QString() : card + "/hwmon/" + entries.first();
}

double microToWatts(const QString &raw) {
    return raw.trimmed().toDouble() / 1e6;
}

QString backupPath(const QString &path) {
    return path + ".bak-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
}

bool writeAtomic(const QString &path, const QString &contents, QString *failure) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (failure)
            *failure = file.errorString();
        return false;
    }
    file.write(contents.toUtf8());
    if (!file.commit()) {
        if (failure)
            *failure = file.errorString();
        return false;
    }
    return true;
}
} // namespace

Backend::Backend(const QString &drmRoot, QObject *parent)
    : QObject(parent), m_root(drmRoot.isEmpty() ? QStringLiteral("/sys/class/drm") : drmRoot) {
    discoverCard();

    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &Backend::poll);
    if (available())
        m_timer.start();
}

// Picks the AMD card with the most VRAM, so a desktop with an APU and a
// discrete card shows the discrete one.
void Backend::discoverCard() {
    const QDir drm(m_root);
    const auto entries = drm.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    const QRegularExpression cardName("^card\\d+$");

    m_cards.clear();
    QString best;
    qint64 bestVram = -1;

    for (const auto &entry : entries) {
        if (!cardName.match(entry).hasMatch())
            continue;
        const QString device = m_root + "/" + entry + "/device";
        if (readFile(device + "/vendor").trimmed() != QLatin1String("0x1002"))
            continue;

        bool ok = false;
        const quint32 id = readFile(device + "/device").trimmed().toUInt(&ok, 0);
        const qint64 vram = readFile(device + "/mem_info_vram_total").trimmed().toLongLong();
        m_cards.append(QVariantMap{
            {"name", ok ? cardNameForDeviceId(id) : tr("AMD GPU")},
            {"path", device},
            {"vramGb", vram / 1073741824.0},
        });
        if (vram > bestVram) {
            bestVram = vram;
            best = device;
        }
    }

    if (best.isEmpty()) {
        m_error = tr("No AMD GPU found under %1").arg(m_root);
        return;
    }
    setCardPath(best);
    poll();
}

QString Backend::activePreset() const {
    const bool atMax = m_powerCapMaxW > 0 && qAbs(m_powerCapW - m_powerCapMaxW) < 3;
    const bool atMin = m_powerCapMinW > 0 && qAbs(m_powerCapW - m_powerCapMinW) < 3;
    const bool atDefault = m_powerCapDefaultW > 0 && qAbs(m_powerCapW - m_powerCapDefaultW) < 3;

    if (atMax && m_perfLevel == QLatin1String("high") &&
        m_powerProfileName.contains(QLatin1String("3D")))
        return "game";
    if (atMin && m_powerProfileName.contains(QLatin1String("POWER_SAVING")))
        return "quiet";
    if (atDefault && m_powerProfileName.contains(QLatin1String("BOOTUP")))
        return "balanced";
    return {};
}

void Backend::applyPreset(const QString &name) {
    if (name != QLatin1String("quiet") && name != QLatin1String("balanced") &&
        name != QLatin1String("game")) {
        setStatus(tr("Unknown preset: %1").arg(name));
        return;
    }
    runPrivileged({"tach", "preset", name}, tr("Preset %1 applied").arg(name));
}

void Backend::selectCard(int index) {
    if (index < 0 || index >= m_cards.size())
        return;
    const QString path = m_cards.at(index).toMap().value("path").toString();
    if (path.isEmpty() || path == m_card)
        return;

    // History belongs to the card that produced it.
    m_utilHistory.clear();
    m_tempHistory.clear();
    m_powerHistory.clear();
    m_processes.clear();
    m_sessionStats.clear();
    m_windowStats.clear();
    m_sumUtil = m_sumPower = m_sumTemp = 0;
    m_maxPower = 0;
    m_maxUtilisation = 0;
    m_maxUtilTemp = 0;
    m_statsSamples = 0;

    setCardPath(path);
    poll();
    emit processesChanged();
    emit historyChanged();
}

void Backend::setCardPath(const QString &cardPath) {
    m_card = cardPath;
    // cardN/device is a symlink into the PCI tree, so the resolved name is the
    // BDF address that /proc/<pid>/fdinfo reports as drm-pdev.
    m_pci = QFileInfo(cardPath).canonicalFilePath().section('/', -1);

    m_controlFile = cardPath + "/power_dpm_force_performance_level";
    if (!QFileInfo::exists(m_controlFile))
        m_controlFile.clear();

    m_profileFile = cardPath + "/pp_power_profile_mode";
    if (!QFileInfo::exists(m_profileFile))
        m_profileFile.clear();

    const QString hwmon = firstHwmon(cardPath);
    m_powerCapFile = hwmon.isEmpty() ? QString() : hwmon + "/power1_cap";
    if (!m_powerCapFile.isEmpty() && !QFileInfo::exists(m_powerCapFile))
        m_powerCapFile.clear();
    if (!m_powerCapFile.isEmpty()) {
        m_powerCapW = microToWatts(readFile(m_powerCapFile));
        m_powerCapMinW = microToWatts(readFile(hwmon + "/power1_cap_min"));
        m_powerCapMaxW = microToWatts(readFile(hwmon + "/power1_cap_max"));
        m_powerCapDefaultW = microToWatts(readFile(hwmon + "/power1_cap_default"));
    }

    // Overdrive only appears when the kernel was booted with the feature mask
    // that unlocks it; without these files there is nothing to control.
    m_canOverdrive = QFileInfo::exists(cardPath + "/pp_od_clk_voltage") ||
                     QFileInfo::exists(cardPath + "/gpu_od");
    m_canFanControl = QFileInfo::exists(cardPath + "/gpu_od/fan_ctrl/fan_curve") ||
                      QFileInfo::exists(cardPath + "/gpu_od/fan_ctrl/fan_speed");

    bool ok = false;
    const quint32 deviceId = readFile(cardPath + "/device").trimmed().toUInt(&ok, 0);
    m_cardName = ok ? cardNameForDeviceId(deviceId) : tr("AMD GPU");
    m_cardIndex = -1;
    for (int i = 0; i < m_cards.size(); ++i) {
        if (m_cards.at(i).toMap().value("path").toString() == cardPath) {
            m_cardIndex = i;
            break;
        }
    }
    m_perfLevel = m_controlFile.isEmpty() ? QString() : readFile(m_controlFile).trimmed();
    m_error.clear();

    detectOverlayTools();
    readFanConfig();
    readProfiles();
    refreshFanState();
    emit cardChanged();
}

QString Backend::hwmonPath() const {
    return firstHwmon(m_card);
}

QString Backend::fanConfigPath() {
    return qEnvironmentVariable("TACH_FAN_CONFIG", QStringLiteral("/etc/tach/fan.json"));
}

// Temps are fixed so the UI stays four sliders instead of a graph editor.
QVariantList Backend::defaultFanCurve() {
    return QVariantList{
        QVariantMap{{"temp", 50}, {"percent", 0}},
        QVariantMap{{"temp", 65}, {"percent", 35}},
        QVariantMap{{"temp", 80}, {"percent", 60}},
        QVariantMap{{"temp", 90}, {"percent", 100}},
    };
}

// Piecewise linear, flat outside the ends. The fan service and the tests share
// this so what the UI promises is exactly what gets written to pwm1.
int Backend::curvePercentAt(const QVariantList &curve, int temperature) {
    if (curve.isEmpty())
        return 50;

    QList<QPair<int, int>> points;
    for (const auto &entry : curve) {
        const QVariantMap map = entry.toMap();
        points.append({map.value("temp").toInt(), map.value("percent").toInt()});
    }
    std::sort(points.begin(), points.end(), [](const auto &a, const auto &b) {
        return a.first < b.first;
    });

    if (temperature <= points.first().first)
        return points.first().second;
    if (temperature >= points.last().first)
        return points.last().second;

    for (int i = 1; i < points.size(); ++i) {
        const auto low = points.at(i - 1);
        const auto high = points.at(i);
        if (temperature <= high.first) {
            const double span = high.first - low.first;
            if (span <= 0)
                return high.second;
            const double t = (temperature - low.first) / span;
            return qRound(low.second + t * (high.second - low.second));
        }
    }
    return points.last().second;
}

void Backend::readFanConfig() {
    QFile file(fanConfigPath());
    if (!file.open(QIODevice::ReadOnly)) {
        // No config yet means the service is not controlling anything.
        m_fanMode = "auto";
        m_fanPercent = 50;
        m_fanCurve = defaultFanCurve();
        emit fanChanged();
        return;
    }
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    m_fanMode = object.value("mode").toString("auto");
    m_fanPercent = object.value("staticPercent").toInt(50);

    QVariantList curve;
    for (const auto &entry : object.value("curve").toArray()) {
        const QJsonObject point = entry.toObject();
        curve.append(QVariantMap{
            {"temp", point.value("temp").toInt()},
            {"percent", point.value("percent").toInt()},
        });
    }
    m_fanCurve = curve.isEmpty() ? defaultFanCurve() : curve;
    emit fanChanged();
}

void Backend::refreshFanState() {
    auto run = [](const QStringList &arguments) {
        QProcess process;
        process.start("systemctl", arguments);
        if (!process.waitForFinished(700))
            return QString();
        return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    };

    const bool serviceActive = run({"is-active", "tach-fan"}) == QLatin1String("active");
    const bool daemonActive = run({"is-active", "power-profiles-daemon"}) == QLatin1String("active");
    const bool managed = QFileInfo::exists(
        "/etc/systemd/system/power-profiles-daemon.service.d/tach.conf");
    const bool profilesActive = run({"is-active", "tach-profiles"}) == QLatin1String("active");

    if (serviceActive != m_fanServiceActive || daemonActive != m_powerDaemonActive ||
        managed != m_dpmManagedByTach) {
        m_fanServiceActive = serviceActive;
        m_powerDaemonActive = daemonActive;
        m_dpmManagedByTach = managed;
        emit fanChanged();
        emit cardChanged();
    }
    if (profilesActive != m_profilesServiceActive) {
        m_profilesServiceActive = profilesActive;
        emit profilesChanged();
    }
}

void Backend::applyFanConfig(const QString &mode, int percent, const QVariantList &curve) {
    QJsonArray points;
    for (const auto &entry : curve) {
        const QVariantMap map = entry.toMap();
        points.append(QJsonObject{
            {"temp", map.value("temp").toInt()},
            {"percent", map.value("percent").toInt()},
        });
    }
    const QJsonObject object{
        {"mode", mode},
        {"staticPercent", qBound(0, percent, 100)},
        {"tempSource", "junction"},
        {"curve", points},
    };
    const QString json = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    runPrivileged({"tach", "fan-apply", json}, tr("Fan settings applied"));
}

void Backend::setFanAuto() {
    runPrivileged({"tach", "fan-auto"}, tr("Fan returned to automatic"));
}

void Backend::takeOverDpm() {
    runPrivileged({"tach", "ppd-takeover"}, tr("Tach now owns the DPM level"));
}

void Backend::restoreDpm() {
    runPrivileged({"tach", "ppd-restore"}, tr("power-profiles-daemon restored"));
}

void Backend::readProfiles() {
    QFile file(Profiles::configPath());
    QJsonObject root;
    if (file.open(QIODevice::ReadOnly))
        root = QJsonDocument::fromJson(file.readAll()).object();

    const Profiles::Config config = Profiles::parseConfig(root);
    QVariantList rules;
    for (const auto &rule : config.rules)
        rules.append(QVariantMap{{"match", rule.match}, {"preset", rule.preset}});

    m_profileAuto = config.autoSwitch;
    m_profileDefault = config.fallback;
    m_profileRules = rules;
    emit profilesChanged();
}

void Backend::saveProfiles(bool autoSwitch, const QString &fallback, const QVariantList &rules) {
    QJsonArray array;
    for (const auto &entry : rules) {
        const QVariantMap map = entry.toMap();
        const QString match = map.value("match").toString().trimmed();
        const QString preset = map.value("preset").toString();
        if (!match.isEmpty())
            array.append(QJsonObject{{"match", match}, {"preset", preset}});
    }
    const QJsonObject root{
        {"auto", autoSwitch},
        {"default", fallback},
        {"rules", array},
    };
    runPrivileged({"tach", "profiles-apply",
                   QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact))},
                  tr("Profiles saved"));
}

void Backend::detectOverlayTools() {
    auto has = [](const QString &binary) {
        return !QStandardPaths::findExecutable(binary).isEmpty();
    };
    m_overlayTools = QVariantMap{
        {"mangohud", has("mangohud")},
        {"vkbasalt", has("vkbasalt")},
        {"gamescope", has("gamescope")},
        {"goverlay", has("goverlay")},
    };
}

void Backend::poll() {
    if (!available())
        return;

    m_utilisation = readFile(m_card + "/gpu_busy_percent").trimmed().toDouble();
    m_memUtilisation = readFile(m_card + "/mem_busy_percent").trimmed().toDouble();
    m_vramUsed = readFile(m_card + "/mem_info_vram_used").trimmed().toLongLong();
    m_vramTotal = readFile(m_card + "/mem_info_vram_total").trimmed().toLongLong();

    const QString hwmon = firstHwmon(m_card);
    if (!hwmon.isEmpty()) {
        m_edgeTemp = readFile(hwmon + "/temp1_input").trimmed().toInt() / 1000;
        m_junctionTemp = readFile(hwmon + "/temp2_input").trimmed().toInt() / 1000;
        m_memTemp = readFile(hwmon + "/temp3_input").trimmed().toInt() / 1000;
        m_power = microToWatts(readFile(hwmon + "/power1_average"));
        m_fanRpm = readFile(hwmon + "/fan1_input").trimmed().toInt();
        m_fanMax = readFile(hwmon + "/fan1_max").trimmed().toInt();
        if (!m_powerCapFile.isEmpty())
            m_powerCapW = microToWatts(readFile(m_powerCapFile));
    }

    m_coreClock = currentClockMhz(readFile(m_card + "/pp_dpm_sclk"));
    m_memClock = currentClockMhz(readFile(m_card + "/pp_dpm_mclk"));
    if (!m_controlFile.isEmpty())
        m_perfLevel = readFile(m_controlFile).trimmed();

    if (!m_profileFile.isEmpty()) {
        int active = -1;
        const QVariantList profiles = parsePowerProfiles(readFile(m_profileFile), &active);
        if (!profiles.isEmpty()) {
            m_powerProfiles = profiles;
            m_powerProfileIndex = active;
            for (const auto &entry : profiles) {
                const QVariantMap map = entry.toMap();
                if (map.value("index").toInt() == active)
                    m_powerProfileName = map.value("name").toString();
            }
        }
    }

    const QString speed = readFile(m_card + "/current_link_speed").trimmed();
    const QString width = readFile(m_card + "/current_link_width").trimmed();
    m_pcie = (speed.isEmpty() || width.isEmpty()) ? QString() : QStringLiteral("%1 x%2").arg(speed, width);

    appendHistory(m_utilHistory, int(m_utilisation));
    appendHistory(m_tempHistory, m_junctionTemp > 0 ? m_junctionTemp : m_edgeTemp);
    appendHistory(m_powerHistory, int(m_power));

    updateStats();
    if (m_recordFile.isOpen()) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        m_recordFile.write(csvRow(now, m_utilisation, m_memUtilisation, vramUsedGb(),
                                  m_edgeTemp, m_junctionTemp, m_memTemp, m_power,
                                  m_fanRpm, m_coreClock, m_memClock)
                               .toUtf8());
        m_recordFile.flush();
        const int seconds = int((now - m_recordStartedMs) / 1000);
        if (seconds != m_recordedSeconds) {
            m_recordedSeconds = seconds;
            emit recordingChanged();
        }
    }

    emit sampleChanged();
    emit historyChanged();

    // Walking every process' fdinfo is the expensive part, so it runs less
    // often than the gauges — but on the first sample too, or the table looks
    // broken at startup. Five seconds keeps idle cost around 1% of a core.
    if (++m_ticks == 1 || m_ticks % 5 == 0)
        refreshProcesses();
    if (m_ticks % 15 == 0)
        refreshFanState();
}

void Backend::updateStats() {
    const int temp = m_junctionTemp > 0 ? m_junctionTemp : m_edgeTemp;
    m_sumUtil += m_utilisation;
    m_sumPower += m_power;
    m_sumTemp += temp;
    ++m_statsSamples;

    m_maxUtilisation = qMax(m_maxUtilisation, int(m_utilisation));
    m_maxUtilTemp = qMax(m_maxUtilTemp, temp);
    m_maxPower = qMax(m_maxPower, m_power);

    auto maxOf = [](const QVariantList &list) {
        int value = 0;
        for (const auto &entry : list)
            value = qMax(value, entry.toInt());
        return value;
    };
    auto avgOf = [](const QVariantList &list) {
        if (list.isEmpty())
            return 0.0;
        double sum = 0;
        for (const auto &entry : list)
            sum += entry.toInt();
        return sum / list.size();
    };

    m_windowStats = QVariantMap{
        {"samples", m_utilHistory.size()},
        {"utilMax", maxOf(m_utilHistory)},
        {"utilAvg", avgOf(m_utilHistory)},
        {"tempMax", maxOf(m_tempHistory)},
        {"tempAvg", avgOf(m_tempHistory)},
        {"powerMax", maxOf(m_powerHistory)},
        {"powerAvg", avgOf(m_powerHistory)},
    };

    m_sessionStats = QVariantMap{
        {"samples", m_statsSamples},
        {"utilMax", m_maxUtilisation},
        {"utilAvg", m_statsSamples > 0 ? m_sumUtil / m_statsSamples : 0.0},
        {"tempMax", m_maxUtilTemp},
        {"tempAvg", m_statsSamples > 0 ? m_sumTemp / m_statsSamples : 0.0},
        {"powerMax", m_maxPower},
        {"powerAvg", m_statsSamples > 0 ? m_sumPower / m_statsSamples : 0.0},
    };
}

QString Backend::csvHeader() {
    return QStringLiteral("timestamp,utilisation,mem_utilisation,vram_gb,edge_c,junction_c,"
                          "mem_c,power_w,fan_rpm,core_mhz,mem_mhz\n");
}

QString Backend::csvRow(qint64 epochMs, double util, double memUtil, double vramUsedGb,
                        int edge, int junction, int memTemp, double power, int fanRpm,
                        int coreClock, int memClock) {
    return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11\n")
        .arg(QDateTime::fromMSecsSinceEpoch(epochMs).toString(Qt::ISODate))
        .arg(util, 0, 'f', 1)
        .arg(memUtil, 0, 'f', 1)
        .arg(vramUsedGb, 0, 'f', 2)
        .arg(edge)
        .arg(junction)
        .arg(memTemp)
        .arg(power, 0, 'f', 1)
        .arg(fanRpm)
        .arg(coreClock)
        .arg(memClock);
}

void Backend::startRecording() {
    if (m_recordFile.isOpen())
        return;

    const QString directory = qEnvironmentVariable(
        "TACH_STATE_DIR",
        QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + "/tach");
    QDir().mkpath(directory);
    m_recordPath = directory + "/session-"
                 + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".csv";

    m_recordFile.setFileName(m_recordPath);
    if (!m_recordFile.open(QIODevice::WriteOnly)) {
        setStatus(tr("Could not start recording: %1").arg(m_recordFile.errorString()));
        return;
    }
    m_recordFile.write(csvHeader().toUtf8());
    m_recordFile.flush();
    m_recordStartedMs = QDateTime::currentMSecsSinceEpoch();
    m_recordedSeconds = 0;

    setStatus(tr("Recording to %1").arg(m_recordPath));
    emit recordingChanged();
}

void Backend::stopRecording() {
    if (!m_recordFile.isOpen())
        return;
    m_recordFile.close();
    setStatus(tr("Saved %1").arg(m_recordPath));
    emit recordingChanged();
}

void Backend::appendHistory(QVariantList &list, int value) {
    list.append(value);
    while (list.size() > HistoryLength)
        list.removeFirst();
}

void Backend::refreshProcesses() {
    if (m_pci.isEmpty())
        return;

    // Aggregate per DRM client first: a process can hold several descriptors
    // of the same client, and summing those would double-count both the VRAM
    // and the engine time.
    QHash<int, QHash<QString, qint64>> vramByClient;
    QHash<int, QHash<QString, qint64>> engineByClient;

    const QDir proc("/proc");
    const auto entries = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        bool ok = false;
        const int pid = entry.toInt(&ok);
        if (!ok)
            continue;
        const QString fdinfoDir = "/proc/" + entry + "/fdinfo";
        const auto fds = QDir(fdinfoDir).entryList(QDir::Files);
        if (fds.isEmpty())
            continue;

        for (const auto &fd : fds) {
            const QString contents = readFile(fdinfoDir + "/" + fd);
            if (contents.isEmpty() || !contents.contains(QLatin1String("drm-client-id")))
                continue;

            bool belongs = m_pci.isEmpty();
            QString client;
            qint64 vram = 0;
            qint64 engine = -1;
            for (const auto &rawLine : contents.split('\n')) {
                const QString line = rawLine.trimmed();
                if (line.startsWith(QLatin1String("drm-pdev:"))) {
                    const QString value = line.section(':', 1).trimmed();
                    belongs = m_pci.isEmpty() || value == m_pci;
                    if (!belongs)
                        break;
                } else if (line.startsWith(QLatin1String("drm-client-id:"))) {
                    client = line.section(':', 1).trimmed();
                } else if (line.startsWith(QLatin1String("drm-total-vram:"))) {
                    vram += parseSize(line.section(':', 1));
                } else if (line.startsWith(QLatin1String("drm-engine-gfx:"))) {
                    engine = qMax(engine, line.section(':', 1).trimmed().section(' ', 0, 0).toLongLong());
                }
            }
            if (!belongs || client.isEmpty())
                continue;
            if (vram > 0)
                vramByClient[pid].insert(client, vram);
            if (engine > 0)
                engineByClient[pid].insert(client, engine);
        }
    }

    const qint64 nowNs = QDateTime::currentMSecsSinceEpoch() * 1000000LL;
    const qint64 elapsed = m_engineStampNs > 0 ? nowNs - m_engineStampNs : 0;

    QList<QPair<int, qint64>> sorted;
    for (auto it = vramByClient.begin(); it != vramByClient.end(); ++it) {
        qint64 total = 0;
        for (auto client = it.value().begin(); client != it.value().end(); ++client)
            total += client.value();
        sorted.append({it.key(), total});
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
        return a.second > b.second;
    });

    QHash<int, qint64> newTotals;
    QVariantList rows;
    for (int i = 0; i < sorted.size() && i < 12; ++i) {
        const int pid = sorted[i].first;
        const qint64 bytes = sorted[i].second;

        qint64 engineTotal = 0;
        const auto engine = engineByClient.value(pid);
        for (auto client = engine.begin(); client != engine.end(); ++client)
            engineTotal += client.value();
        if (engineTotal > 0)
            newTotals.insert(pid, engineTotal);

        double busy = 0;
        if (elapsed > 0 && m_engineTotals.contains(pid) && engineTotal > 0) {
            const qint64 delta = engineTotal - m_engineTotals.value(pid);
            if (delta > 0)
                busy = qBound(0.0, double(delta) / double(elapsed) * 100.0, 100.0);
        }

        // comm is capped at 15 characters; the exe symlink carries the real name.
        QString name = QFileInfo(QString("/proc/%1/exe").arg(pid)).symLinkTarget();
        if (!name.isEmpty()) {
            name = name.section('/', -1);
            if (name.endsWith(QLatin1String(" (deleted)")))
                name.chop(10);
        }
        if (name.isEmpty())
            name = readFile(QString("/proc/%1/comm").arg(pid)).trimmed();
        rows.append(QVariantMap{
            {"pid", pid},
            {"name", name.isEmpty() ? tr("unknown") : name},
            {"vram", bytes},
            {"busy", busy},
        });
    }

    m_engineTotals = newTotals;
    m_engineStampNs = nowNs;

    m_processes = rows;
    emit processesChanged();
}

void Backend::runPrivileged(const QStringList &arguments, const QString &successMessage) {
    if (m_busy)
        return;

    setStatus(tr("Asking for permission…"), true);

    auto *process = new QProcess(this);
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        process->deleteLater();
        if (error == QProcess::FailedToStart)
            setStatus(tr("Could not run pkexec — is tach installed?"));
    });
    connect(process, &QProcess::finished, this,
            [this, process, successMessage](int code, QProcess::ExitStatus status) {
                const QString stderrText = QString::fromUtf8(process->readAllStandardError()).trimmed();
                process->deleteLater();
                if (code == 0 && status == QProcess::NormalExit) {
                    setStatus(successMessage);
                    poll();
                    readFanConfig();
                    refreshFanState();
                } else if (code == 126) {
                    setStatus(tr("Permission denied"));
                } else {
                    setStatus(stderrText.isEmpty() ? tr("The change was not applied") : stderrText);
                }
            });
    process->start("pkexec", arguments);
}

void Backend::setPerformanceLevel(const QString &level) {
    if (!kLevels.contains(level)) {
        setStatus(tr("Unknown performance level: %1").arg(level));
        return;
    }
    if (!canControl()) {
        setStatus(tr("This GPU does not expose a performance level"));
        return;
    }
    runPrivileged({"tach", "set-level", level}, tr("Performance level set to %1").arg(level));
}

void Backend::setPowerProfile(int index) {
    if (index < 0) {
        setStatus(tr("Pick a power profile first"));
        return;
    }
    runPrivileged({"tach", "set-profile", QString::number(index)}, tr("Power profile changed"));
}

void Backend::setPowerCap(double watts) {
    if (!canPowerCap()) {
        setStatus(tr("This GPU does not expose a power limit"));
        return;
    }
    const double clamped = qBound(m_powerCapMinW, watts, m_powerCapMaxW);
    runPrivileged({"tach", "set-power-cap", QString::number(qRound(clamped))},
                  tr("Power limit set to %1 W").arg(qRound(clamped)));
}

QString Backend::overdriveHint() const {
    if (m_canOverdrive)
        return {};
    return tr("Add amdgpu.ppfeaturemask=0xffffffff to the kernel command line and reboot to unlock "
              "clock, voltage and fan controls.");
}

void Backend::setStatus(const QString &message, bool busy) {
    m_status = message;
    m_busy = busy;
    emit statusChanged();
}

QString Backend::readFile(const QString &path) const {
    if (path.isEmpty())
        return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

// The active clock is the line marked with a trailing asterisk.
int Backend::currentClockMhz(const QString &table) {
    static const QRegularExpression number("(\\d+)\\s*Mhz", QRegularExpression::CaseInsensitiveOption);
    int fallback = 0;
    for (const auto &rawLine : table.split('\n')) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        const auto match = number.match(line);
        if (!match.hasMatch())
            continue;
        const int value = match.captured(1).toInt();
        if (line.endsWith('*'))
            return value;
        fallback = value;
    }
    return fallback;
}

// fdinfo prints sizes as "0" or "123 MiB"; the kernel never emits decimals.
qint64 Backend::parseSize(const QString &text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return 0;
    static const QRegularExpression pattern(R"(^(\d+)\s*(KiB|MiB|GiB|B)?$)", QRegularExpression::CaseInsensitiveOption);
    const auto match = pattern.match(trimmed);
    if (!match.hasMatch())
        return 0;
    const qint64 value = match.captured(1).toLongLong();
    const QString unit = match.captured(2).toLower();
    if (unit == "kib")
        return value * 1024;
    if (unit == "mib")
        return value * 1024 * 1024;
    if (unit == "gib")
        return value * 1024LL * 1024 * 1024;
    return value;
}

QString Backend::cardNameForDeviceId(quint32 deviceId) {
    switch (deviceId) {
    case 0x744c:
        return QStringLiteral("Radeon RX 7900 XT/XTX");
    case 0x73bf:
        return QStringLiteral("Radeon RX 6800/6800 XT/6900 XT");
    case 0x73df:
        return QStringLiteral("Radeon RX 6700 XT");
    case 0x73ef:
        return QStringLiteral("Radeon RX 6950 XT");
    case 0x747e:
        return QStringLiteral("Radeon RX 7800 XT");
    case 0x73ff:
        return QStringLiteral("Radeon RX 7600");
    case 0x164e:
        return QStringLiteral("Radeon 610M");
    default:
        return QStringLiteral("AMD GPU %1").arg(QString::number(deviceId, 16).rightJustified(4, '0'));
    }
}

// Sums the VRAM a single DRM client file descriptor holds, if it belongs to
// this PCI device. Returns 0 for other GPUs and for clients that hold none.
qint64 Backend::vramForProcess(const QString &fdinfo, const QString &pciAddress) {
    bool belongs = pciAddress.isEmpty();
    qint64 vram = 0;
    for (const auto &rawLine : fdinfo.split('\n')) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QLatin1String("drm-pdev:"))) {
            const QString value = line.section(':', 1).trimmed();
            belongs = pciAddress.isEmpty() || value == pciAddress;
            if (!belongs)
                return 0;
        } else if (line.startsWith(QLatin1String("drm-total-vram:"))) {
            vram += parseSize(line.section(':', 1));
        }
    }
    return belongs ? vram : 0;
}

QHash<QString, qint64> Backend::engineTimeForProcess(const QString &fdinfo, const QString &pciAddress) {
    QHash<QString, qint64> result;
    bool belongs = pciAddress.isEmpty();
    QString client;
    qint64 engine = -1;
    for (const auto &rawLine : fdinfo.split('\n')) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QLatin1String("drm-pdev:"))) {
            const QString value = line.section(':', 1).trimmed();
            belongs = pciAddress.isEmpty() || value == pciAddress;
            if (!belongs)
                return result;
        } else if (line.startsWith(QLatin1String("drm-client-id:"))) {
            client = line.section(':', 1).trimmed();
        } else if (line.startsWith(QLatin1String("drm-engine-gfx:"))) {
            engine = qMax(engine, line.section(':', 1).trimmed().section(' ', 0, 0).toLongLong());
        }
    }
    if (belongs && !client.isEmpty() && engine > 0)
        result.insert(client, engine);
    return result;
}

QStringList Backend::performanceLevels() {
    return kLevels;
}

// The table looks like:
//   PROFILE_INDEX(NAME) CLOCK_TYPE(NAME) FPS …
//    0 BOOTUP_DEFAULT*:
//                       0(       GFXCLK) …
//    1 3D_FULL_SCREEN :
QVariantList Backend::parsePowerProfiles(const QString &table, int *activeIndex) {
    QVariantList profiles;
    static const QRegularExpression entry(R"(^\s*(\d+)\s+([A-Z0-9_]+)\s*(\*)?\s*:)");
    if (activeIndex)
        *activeIndex = -1;

    for (const auto &rawLine : table.split('\n')) {
        const auto match = entry.match(rawLine);
        if (!match.hasMatch())
            continue;
        const int index = match.captured(1).toInt();
        const QString name = match.captured(2);
        const bool active = !match.captured(3).isEmpty();
        profiles.append(QVariantMap{{"index", index}, {"name", name}, {"active", active}});
        if (active && activeIndex)
            *activeIndex = index;
    }
    return profiles;
}

QString Backend::mergeManagedBlock(const QString &existing, const QString &block) {
    QString out = existing;
    const int start = out.indexOf(kBlockBegin);
    if (start >= 0) {
        const int end = out.indexOf(kBlockEnd, start);
        if (end >= 0)
            out.remove(start, end - start + kBlockEnd.length());
        else
            out.remove(start, out.length() - start);
    }
    out = out.trimmed();
    if (!out.isEmpty())
        out += "\n\n";
    out += block.trimmed() + "\n";
    return out;
}

QString Backend::writeMangoHudPreset(const QString &preset) {
    static const QMap<QString, QString> presets{
        {"minimal", "fps\ngpu_temp\nvram\nframe_timing=0\n"},
        {"compact", "fps\ngpu_stats\ngpu_temp\ngpu_clock\nvram\ncpu_stats\nframe_timing\n"},
        {"full", "fps\ngpu_stats\ngpu_temp\ngpu_junction_temp\ngpu_mem_temp\ngpu_clock\ngpu_power\n"
                 "vram\ncpu_stats\ncpu_temp\nram\nfan\nframe_timing\ntime\ngpu_name\n"},
    };
    if (!presets.contains(preset))
        return tr("Unknown preset");

    const QString path = QDir::homePath() + "/.config/MangoHud/MangoHud.conf";
    const QString block = kBlockBegin + "\n" + presets.value(preset) + kBlockEnd;
    const QString existing = readFile(path);
    if (!existing.isEmpty()) {
        QString failure;
        if (!writeAtomic(backupPath(path), existing, &failure))
            return tr("Could not back up %1: %2").arg(path, failure);
    }
    QString failure;
    if (!writeAtomic(path, mergeManagedBlock(existing, block), &failure))
        return tr("Could not write %1: %2").arg(path, failure);

    detectOverlayTools();
    const QString message = tr("Wrote the %1 MangoHud preset to %2").arg(preset, path);
    setStatus(message);
    return message;
}

QString Backend::writeVkBasaltPreset(const QString &preset) {
    static const QMap<QString, QString> presets{
        {"fxaa", "effects = fxaa\nfxaaQualitySubpix = 0.75\n"},
        {"smaa", "effects = smaa\nsmaaEdgeDetection = luma\n"},
        {"cas", "effects = cas\ncasSharpness = 0.4\n"},
        {"dls", "effects = dls\ndlsSharpness = 0.5\ndlsDenoise = 0.17\ndlsVMStrength = 0.05\n"},
    };
    if (!presets.contains(preset))
        return tr("Unknown preset");

    const QString path = QDir::homePath() + "/.config/vkBasalt/vkBasalt.conf";
    const QString block = kBlockBegin + "\nenableOnLaunch = True\n" + presets.value(preset) + kBlockEnd;
    const QString existing = readFile(path);
    if (!existing.isEmpty()) {
        QString failure;
        if (!writeAtomic(backupPath(path), existing, &failure))
            return tr("Could not back up %1: %2").arg(path, failure);
    }
    QString failure;
    if (!writeAtomic(path, mergeManagedBlock(existing, block), &failure))
        return tr("Could not write %1: %2").arg(path, failure);

    detectOverlayTools();
    const QString message = tr("Wrote the %1 vkBasalt preset to %2").arg(preset, path);
    setStatus(message);
    return message;
}

// There is no global FSR switch on Linux: Radeon Super Resolution lives in
// gamescope, and MangoHud and vkBasalt are enabled per launch through the
// environment. These strings are what belongs in a game's launch options.
QString Backend::launchOptions(const QString &tool) {
    if (tool == "mangohud")
        return QStringLiteral("mangohud %command%");
    if (tool == "vkbasalt")
        return QStringLiteral("ENABLE_VKBASALT=1 %command%");
    if (tool == "fsr")
        return QStringLiteral("gamescope -W 2560 -H 1440 -w 1920 -h 1080 -F fsr -f -- %command%");
    if (tool == "everything")
        return QStringLiteral("ENABLE_VKBASALT=1 mangohud gamescope -W 2560 -H 1440 -w 1920 -h 1080 -F fsr -f -- %command%");
    return {};
}

void Backend::openUrl(const QString &url) {
    if (!url.startsWith(QLatin1String("https://")))
        return;
    QDesktopServices::openUrl(QUrl(url));
}

void Backend::copyToClipboard(const QString &text) {
    if (!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        QProcess process;
        process.start("wl-copy", {"--type", "text/plain"});
        if (process.waitForStarted(1500)) {
            process.write(text.toUtf8());
            process.closeWriteChannel();
            if (process.waitForFinished(1500) && process.exitCode() == 0) {
                setStatus(tr("Copied"));
                return;
            }
        }
    }
    QGuiApplication::clipboard()->setText(text);
    setStatus(tr("Copied"));
}
