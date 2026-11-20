#include "backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>

namespace {
const QStringList kLevels{"auto", "high", "low"};

QString firstHwmon(const QString &card) {
    const QDir dir(card + "/hwmon");
    const auto entries = dir.entryList({"hwmon*"}, QDir::Dirs | QDir::NoDotAndDotDot);
    return entries.isEmpty() ? QString() : card + "/hwmon/" + entries.first();
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

    QString best;
    qint64 bestVram = -1;
    for (const auto &entry : entries) {
        if (!cardName.match(entry).hasMatch())
            continue;
        const QString device = m_root + "/" + entry + "/device";
        if (readFile(device + "/vendor").trimmed() != QLatin1String("0x1002"))
            continue;
        const qint64 vram = readFile(device + "/mem_info_vram_total").trimmed().toLongLong();
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

void Backend::setCardPath(const QString &cardPath) {
    m_card = cardPath;
    // cardN/device is a symlink into the PCI tree, so the resolved name is the
    // BDF address that /proc/<pid>/fdinfo reports as drm-pdev.
    m_pci = QFileInfo(cardPath).canonicalFilePath().section('/', -1);
    m_controlFile = cardPath + "/power_dpm_force_performance_level";
    if (!QFileInfo::exists(m_controlFile))
        m_controlFile.clear();

    bool ok = false;
    const quint32 deviceId = readFile(cardPath + "/device").trimmed().toUInt(&ok, 0);
    m_cardName = ok ? cardNameForDeviceId(deviceId) : tr("AMD GPU");
    m_perfLevel = readFile(m_controlFile).trimmed();
    m_error.clear();

    if (m_utilHistory.isEmpty())
    emit cardChanged();
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
        m_power = readFile(hwmon + "/power1_average").trimmed().toDouble() / 1e6;
        m_fanRpm = readFile(hwmon + "/fan1_input").trimmed().toInt();
        m_fanMax = readFile(hwmon + "/fan1_max").trimmed().toInt();
    }

    m_coreClock = currentClockMhz(readFile(m_card + "/pp_dpm_sclk"));
    m_memClock = currentClockMhz(readFile(m_card + "/pp_dpm_mclk"));
    m_perfLevel = readFile(m_controlFile).trimmed();

    const QString speed = readFile(m_card + "/current_link_speed").trimmed();
    const QString width = readFile(m_card + "/current_link_width").trimmed();
    m_pcie = (speed.isEmpty() || width.isEmpty()) ? QString() : QStringLiteral("%1 x%2").arg(speed, width);

    appendHistory(m_utilHistory, int(m_utilisation));
    appendHistory(m_tempHistory, m_junctionTemp > 0 ? m_junctionTemp : m_edgeTemp);
    appendHistory(m_powerHistory, int(m_power));

    emit sampleChanged();
    emit historyChanged();

    // Walking every process' fdinfo is the expensive part, so it runs less
    // often than the gauges — but on the first sample too, or the table looks
    // broken for the first two seconds.
    if (++m_ticks == 1 || m_ticks % 5 == 0)
        refreshProcesses();
}

void Backend::appendHistory(QVariantList &list, int value) {
    list.append(value);
    while (list.size() > HistoryLength)
        list.removeFirst();
}

void Backend::refreshProcesses() {
    if (m_pci.isEmpty())
        return;

    QHash<int, qint64> byPid;
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
        qint64 total = 0;
        for (const auto &fd : fds)
            total += vramForProcess(readFile(fdinfoDir + "/" + fd), m_pci);
        if (total > 0)
            byPid.insert(pid, total);
    }

    QList<QPair<int, qint64>> sorted;
    sorted.reserve(byPid.size());
    for (auto it = byPid.begin(); it != byPid.end(); ++it)
        sorted.append({it.key(), it.value()});
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
        return a.second > b.second;
    });

    QVariantList rows;
    for (int i = 0; i < sorted.size() && i < 8; ++i) {
        const int pid = sorted[i].first;
        const qint64 bytes = sorted[i].second;
        const QString name = readFile(QString("/proc/%1/comm").arg(pid)).trimmed();
        rows.append(QVariantMap{
            {"pid", pid},
            {"name", name.isEmpty() ? tr("unknown") : name},
            {"vram", bytes},
        });
    }
    m_processes = rows;
    emit processesChanged();
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
    if (m_busy)
        return;

    setStatus(tr("Asking for permission to set %1…").arg(level), true);

    auto *process = new QProcess(this);
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        process->deleteLater();
        if (error == QProcess::FailedToStart)
            setStatus(tr("Could not run pkexec — is tach installed?"));
    });
    connect(process, &QProcess::finished, this, [this, process, level](int code, QProcess::ExitStatus status) {
        const QString stderrText = QString::fromUtf8(process->readAllStandardError()).trimmed();
        process->deleteLater();
        if (code == 0 && status == QProcess::NormalExit) {
            setStatus(tr("Performance level set to %1").arg(level));
            poll();
        } else if (code == 126) {
            setStatus(tr("Permission denied"));
        } else {
            setStatus(stderrText.isEmpty() ? tr("Could not set the performance level") : stderrText);
        }
    });
    process->start("pkexec", {"tach", "set-level", level});
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

QStringList Backend::performanceLevels() {
    return kLevels;
}
