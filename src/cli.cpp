#include "cli.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTextStream>

#include "backend.h"
#include "profiles.h"

#include <csignal>
#include <cstdio>

namespace {

QTextStream &out() {
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err() {
    static QTextStream stream(stderr);
    return stream;
}

int statusCommand(const QStringList &args) {
    Backend backend;
    backend.poll();
    if (!backend.available()) {
        err() << backend.error() << "\n";
        return 1;
    }

    if (args.contains("--json")) {
        QJsonObject object;
        object["card"] = backend.cardName();
        object["card_path"] = backend.cardPath();
        object["utilisation"] = backend.utilisation();
        object["mem_utilisation"] = backend.memUtilisation();
        object["vram_used_gb"] = backend.vramUsedGb();
        object["vram_total_gb"] = backend.vramTotalGb();
        object["edge_temp_c"] = backend.edgeTemp();
        object["junction_temp_c"] = backend.junctionTemp();
        object["mem_temp_c"] = backend.memTemp();
        object["power_w"] = backend.power();
        object["fan_rpm"] = backend.fanRpm();
        object["fan_max_rpm"] = backend.fanMax();
        object["core_clock_mhz"] = backend.coreClock();
        object["mem_clock_mhz"] = backend.memClock();
        object["pcie"] = backend.pcie();
        object["performance_level"] = backend.perfLevel();
        object["power_profile"] = backend.powerProfile();
        object["power_profile_index"] = backend.powerProfileIndex();
        object["power_cap_w"] = backend.powerCapW();
        object["overdrive_available"] = backend.canOverdrive();
        out() << QJsonDocument(object).toJson(QJsonDocument::Compact) << "\n";
        return 0;
    }

    out() << backend.cardName() << "  (" << backend.cardPath() << ")\n";
    out() << "  utilisation    " << backend.utilisation() << " %\n";
    out() << "  vram           " << backend.vramUsedGb() << " / " << backend.vramTotalGb() << " GB\n";
    out() << "  temperature    " << backend.edgeTemp() << " / " << backend.junctionTemp() << " / "
          << backend.memTemp() << " C (edge/junction/mem)\n";
    out() << "  power          " << backend.power() << " W (limit " << backend.powerCapW() << " W)\n";
    out() << "  fan            " << backend.fanRpm() << " rpm (max " << backend.fanMax() << ")\n";
    out() << "  clocks         " << backend.coreClock() << " / " << backend.memClock() << " MHz\n";
    out() << "  pcie           " << backend.pcie() << "\n";
    out() << "  performance    " << backend.perfLevel() << "\n";
    out() << "  power profile  " << backend.powerProfile() << " (" << backend.powerProfileIndex() << ")\n";
    if (!backend.canOverdrive())
        out() << "  overdrive      unavailable (boot with amdgpu.ppfeaturemask=0xffffffff)\n";
    out().flush();
    return 0;
}

int setLevelCommand(const QStringList &args) {
    const QString level = args.value(1);
    if (!Backend::performanceLevels().contains(level)) {
        err() << "usage: tach set-level <auto|high|low>\n";
        return 2;
    }

    Backend backend;
    if (!backend.available()) {
        err() << backend.error() << "\n";
        return 1;
    }
    if (!backend.canControl()) {
        err() << "this GPU does not expose a performance level\n";
        return 1;
    }

    QFile file(backend.cardPath() + "/power_dpm_force_performance_level");
    if (!file.open(QIODevice::WriteOnly)) {
        err() << "cannot write " << file.fileName() << " (run under pkexec)\n";
        return 1;
    }
    file.write(level.toUtf8());
    file.close();
    out() << level << "\n";
    out().flush();
    return 0;
}

int setProfileCommand(const QStringList &args) {
    bool ok = false;
    const int requested = args.value(1).toInt(&ok);
    if (!ok) {
        err() << "usage: tach set-profile <index>\n";
        return 2;
    }

    Backend backend;
    if (!backend.available()) {
        err() << backend.error() << "\n";
        return 1;
    }

    int active = -1;
    QFile table(backend.cardPath() + "/pp_power_profile_mode");
    if (!table.open(QIODevice::ReadOnly)) {
        err() << "this GPU does not expose power profiles\n";
        return 1;
    }
    const QVariantList profiles = Backend::parsePowerProfiles(QString::fromUtf8(table.readAll()), &active);
    table.close();

    bool known = false;
    for (const auto &entry : profiles)
        if (entry.toMap().value("index").toInt() == requested)
            known = true;
    if (!known) {
        err() << "no power profile with index " << requested << "\n";
        return 2;
    }

    QFile file(table.fileName());
    if (!file.open(QIODevice::WriteOnly)) {
        err() << "cannot write " << file.fileName() << " (run under pkexec)\n";
        return 1;
    }
    file.write(QByteArray::number(requested));
    file.close();
    out() << "profile " << requested << "\n";
    out().flush();
    return 0;
}

int setPowerCapCommand(const QStringList &args) {
    bool ok = false;
    const double watts = args.value(1).toDouble(&ok);
    if (!ok) {
        err() << "usage: tach set-power-cap <watts>\n";
        return 2;
    }

    Backend backend;
    if (!backend.available()) {
        err() << backend.error() << "\n";
        return 1;
    }
    if (!backend.canPowerCap()) {
        err() << "this GPU does not expose a power limit\n";
        return 1;
    }

    const double clamped = qBound(backend.powerCapMinW(), watts, backend.powerCapMaxW());
    const double micro = clamped * 1e6;

    const QString hwmonRoot = backend.cardPath() + "/hwmon";
    const QDir hwmonDir(hwmonRoot);
    const auto entries = hwmonDir.entryList({"hwmon*"}, QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.isEmpty()) {
        err() << "power cap file not found\n";
        return 1;
    }
    QFile file(hwmonRoot + "/" + entries.first() + "/power1_cap");
    if (!file.open(QIODevice::WriteOnly)) {
        err() << "cannot write " << file.fileName() << " (run under pkexec)\n";
        return 1;
    }
    file.write(QByteArray::number(qRound64(micro)));
    file.close();
    out() << qRound(clamped) << " W\n";
    out().flush();
    return 0;
}

// ---------------------------------------------------------------- fan ----

volatile std::sig_atomic_t gStop = 0;

void requestStop(int) {
    gStop = 1;
}

bool writeSysfs(const QString &path, const QString &value, const QString &why) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        err() << why << ": cannot write " << path << "\n";
        return false;
    }
    file.write(value.toUtf8());
    file.close();
    return true;
}

int readInt(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    return QString::fromUtf8(file.readAll()).trimmed().toInt();
}

QString hwmonFor(const QString &card) {
    const QDir dir(card + "/hwmon");
    const auto entries = dir.entryList({"hwmon*"}, QDir::Dirs | QDir::NoDotAndDotDot);
    return entries.isEmpty() ? QString() : card + "/hwmon/" + entries.first();
}

// The GUI never writes pwm1 itself: this loop owns the fan, so closing the
// window cannot leave it pinned at whatever it was last set to. systemd
// restarts it on failure and ExecStopPost / the guard below restore automatic
// control on the way out.
int runFanService() {
    // TACH_HWMON exists so the tests can drive the loop against a fake tree;
    // in normal use the card is discovered the same way the GUI does it.
    QString hwmon = qEnvironmentVariable("TACH_HWMON");
    if (hwmon.isEmpty()) {
        Backend backend;
        if (!backend.available()) {
            err() << backend.error() << "\n";
            return 1;
        }
        hwmon = hwmonFor(backend.cardPath());
    }
    if (hwmon.isEmpty()) {
        err() << "no hwmon directory\n";
        return 1;
    }

    const QString pwm = hwmon + "/pwm1";
    const QString enable = hwmon + "/pwm1_enable";
    const QString edge = hwmon + "/temp1_input";
    const QString junction = hwmon + "/temp2_input";
    const QString configPath = Backend::fanConfigPath();

    std::signal(SIGTERM, requestStop);
    std::signal(SIGINT, requestStop);

    struct Guard {
        QString path;
        ~Guard() {
            QFile file(path);
            if (file.open(QIODevice::WriteOnly))
                file.write("2");
        }
    } guard{enable};

    int lastPercent = -1;
    int lastMode = -1; // 0 = automatic, 1 = manual
    int failures = 0;

    while (!gStop) {
        QJsonObject config;
        QFile file(configPath);
        if (file.open(QIODevice::ReadOnly))
            config = QJsonDocument::fromJson(file.readAll()).object();

        const QString mode = config.value("mode").toString("auto");
        const int staticPercent = config.value("staticPercent").toInt(50);
        QVariantList curve;
        for (const auto &entry : config.value("curve").toArray()) {
            const QJsonObject point = entry.toObject();
            curve.append(QVariantMap{
                {"temp", point.value("temp").toInt()},
                {"percent", point.value("percent").toInt()},
            });
        }
        if (curve.isEmpty())
            curve = Backend::defaultFanCurve();

        int temp = readInt(junction) / 1000;
        if (temp <= 0)
            temp = readInt(edge) / 1000;
        if (temp <= 0) {
            if (++failures >= 3) {
                err() << "cannot read a temperature, returning the fan to automatic\n";
                return 1;
            }
            QThread::msleep(500);
            continue;
        }
        failures = 0;

        if (mode == QLatin1String("auto")) {
            if (lastMode != 0) {
                writeSysfs(enable, "2", "fan service");
                lastMode = 0;
                lastPercent = -1;
            }
        } else {
            int target = mode == QLatin1String("curve") ? Backend::curvePercentAt(curve, temp)
                                                        : staticPercent;
            // Never let the software curve stop the fan on a hot card.
            if (temp >= 55 && target < 20)
                target = 20;
            target = qBound(0, target, 100);

            if (lastMode != 1) {
                writeSysfs(enable, "1", "fan service");
                lastMode = 1;
            }
            if (target != lastPercent) {
                const int raw = qRound(target * 255.0 / 100.0);
                if (writeSysfs(pwm, QString::number(raw), "fan service"))
                    std::fprintf(stderr, "tach-fan: %d C -> %d %%\n", temp, target);
                lastPercent = target;
            }
        }

        for (int i = 0; i < 20 && !gStop; ++i)
            QThread::msleep(100);
    }
    return 0;
}

// A headless sample log for benchmarks: one JSON object per line, or a CSV
// file, driven without an event loop.
int logCommand(const QStringList &args) {
    int seconds = 10;
    int interval = 1000;
    QString csvPath;

    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i) == "--seconds" && i + 1 < args.size())
            seconds = args.at(++i).toInt();
        else if (args.at(i) == "--interval" && i + 1 < args.size())
            interval = qMax(100, args.at(++i).toInt());
        else if (args.at(i) == "--csv" && i + 1 < args.size())
            csvPath = args.at(++i);
    }
    if (seconds <= 0) {
        err() << "--seconds must be positive\n";
        return 2;
    }

    Backend backend;
    if (!backend.available()) {
        err() << backend.error() << "\n";
        return 1;
    }

    QFile csv;
    if (!csvPath.isEmpty()) {
        csv.setFileName(csvPath);
        if (!csv.open(QIODevice::WriteOnly)) {
            err() << "cannot write " << csvPath << "\n";
            return 1;
        }
        csv.write(Backend::csvHeader().toUtf8());
    }

    const int samples = qMax(1, seconds * 1000 / interval);
    double sumPower = 0;
    int maxTemp = 0;
    for (int i = 0; i < samples; ++i) {
        backend.poll();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        if (csv.isOpen()) {
            csv.write(Backend::csvRow(now, backend.utilisation(), backend.memUtilisation(),
                                      backend.vramUsedGb(), backend.edgeTemp(),
                                      backend.junctionTemp(), backend.memTemp(), backend.power(),
                                      backend.fanRpm(), backend.coreClock(), backend.memClock())
                          .toUtf8());
            csv.flush();
        } else {
            QJsonObject sample{
                {"t", QDateTime::fromMSecsSinceEpoch(now).toString(Qt::ISODate)},
                {"utilisation", backend.utilisation()},
                {"vram_gb", backend.vramUsedGb()},
                {"edge_c", backend.edgeTemp()},
                {"junction_c", backend.junctionTemp()},
                {"mem_c", backend.memTemp()},
                {"power_w", backend.power()},
                {"fan_rpm", backend.fanRpm()},
                {"core_mhz", backend.coreClock()},
                {"mem_mhz", backend.memClock()},
            };
            out() << QJsonDocument(sample).toJson(QJsonDocument::Compact) << "\n";
            out().flush();
        }

        sumPower += backend.power();
        maxTemp = qMax(maxTemp, backend.junctionTemp());

        if (i + 1 < samples)
            QThread::msleep(interval);
    }
    if (csv.isOpen()) {
        csv.close();
        out() << csvPath << "\n";
        out().flush();
    }

    err() << "avg power " << QString::number(sumPower / samples, 'f', 1)
          << " W, max junction " << maxTemp << " C\n";
    return 0;
}

int fanApplyCommand(const QStringList &args) {
    const QString json = args.value(1);
    if (json.isEmpty()) {
        err() << "usage: tach fan-apply '<json>'\n";
        return 2;
    }
    const QJsonObject object = QJsonDocument::fromJson(json.toUtf8()).object();
    if (!object.contains("mode")) {
        err() << "fan config needs a mode\n";
        return 2;
    }

    const QString path = Backend::fanConfigPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        err() << "cannot write " << path << " (run under pkexec)\n";
        return 1;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    file.close();

    // The unit ships with the package; enabling it here is the one-time step.
    QProcess::execute("systemctl", {"enable", "tach-fan"});
    const int code = QProcess::execute("systemctl", {"restart", "tach-fan"});
    if (code != 0) {
        err() << "config written, but tach-fan.service did not start\n";
        return 1;
    }
    out() << "applied\n";
    out().flush();
    return 0;
}

int fanAutoCommand() {
    Backend backend;
    if (!backend.available()) {
        err() << backend.error() << "\n";
        return 1;
    }
    const QString hwmon = hwmonFor(backend.cardPath());
    if (hwmon.isEmpty()) {
        err() << "no hwmon directory\n";
        return 1;
    }
    if (!writeSysfs(hwmon + "/pwm1_enable", "2", "fan auto"))
        return 1;

    // Keep the config honest so the next start does not re-take the fan.
    const QString path = Backend::fanConfigPath();
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(QJsonObject{{"mode", "auto"}}).toJson());
        file.close();
    }
    out() << "auto\n";
    out().flush();
    return 0;
}

// power-profiles-daemon writes the amdgpu DPM level from its own profile, which
// fights the Performance level control. The override keeps it out of the way
// without touching anything else the daemon does.
// A preset is one privileged step that sets the power cap, the DPM level, the
// power profile and the fan curve together. The same code runs from the
// automatic profile watcher.
int presetCommand(const QStringList &args) {
    const QString name = args.value(1);
    Backend backend;
    if (!backend.available()) {
        err() << backend.error() << "\n";
        return 1;
    }
    const int applied = Profiles::applyPreset(name, backend);
    if (applied < 0) {
        err() << "usage: tach preset <quiet|balanced|game>\n";
        return 2;
    }
    if (applied == 0) {
        err() << "nothing could be applied\n";
        return 1;
    }
    out() << name << " (" << applied << " settings)\n";
    out().flush();
    return 0;
}

int ppdCommand(bool restore) {
    const QString dir = "/etc/systemd/system/power-profiles-daemon.service.d";
    const QString override = dir + "/tach.conf";

    if (restore) {
        QFile::remove(override);
        QDir().rmdir(dir);
        QProcess::execute("systemctl", {"daemon-reload"});
        QProcess::execute("systemctl", {"restart", "power-profiles-daemon"});
        out() << "restored\n";
        out().flush();
        return 0;
    }

    QString executable = "/usr/lib/power-profiles-daemon";
    QFile unit("/usr/lib/systemd/system/power-profiles-daemon.service");
    if (unit.open(QIODevice::ReadOnly)) {
        for (const auto &line : QString::fromUtf8(unit.readAll()).split('\n'))
            if (line.startsWith("ExecStart="))
                executable = line.mid(10).trimmed().section(' ', 0, 0);
    }

    QDir().mkpath(dir);
    QFile file(override);
    if (!file.open(QIODevice::WriteOnly)) {
        err() << "cannot write " << override << " (run under pkexec)\n";
        return 1;
    }
    file.write(QString("[Service]\n"
                       "ExecStart=\n"
                       "ExecStart=%1 --block-action=amdgpu_dpm\n")
                   .arg(executable)
                   .toUtf8());
    file.close();

    QProcess::execute("systemctl", {"daemon-reload"});
    if (QProcess::execute("systemctl", {"restart", "power-profiles-daemon"}) != 0) {
        err() << "override written, but power-profiles-daemon did not restart\n";
        return 1;
    }
    out() << "dpm owned by tach\n";
    out().flush();
    return 0;
}

} // namespace

int runCli(const QStringList &args) {
    const QString command = args.value(0);
    if (command == "status")
        return statusCommand(args);
    if (command == "set-level")
        return setLevelCommand(args);
    if (command == "set-profile")
        return setProfileCommand(args);
    if (command == "set-power-cap")
        return setPowerCapCommand(args);
    if (command == "log")
        return logCommand(args);
    if (command == "profiles-apply")
        return Profiles::applyConfig(args.value(1));
    if (command == "--profile-watch")
        return Profiles::runWatch();
    if (command == "preset")
        return presetCommand(args);
    if (command == "fan-apply")
        return fanApplyCommand(args);
    if (command == "fan-auto")
        return fanAutoCommand();
    if (command == "--fan-service")
        return runFanService();
    if (command == "ppd-takeover")
        return ppdCommand(false);
    if (command == "ppd-restore")
        return ppdCommand(true);
    err() << "usage: tach status [--json] | tach set-level <auto|high|low> | "
             "tach set-profile <index> | tach set-power-cap <watts> | "
             "tach log [--seconds N] [--csv FILE] | tach preset <quiet|balanced|game> | "
             "tach fan-apply '<json>' | "
             "tach fan-auto | tach ppd-takeover | tach ppd-restore\n";
    return 2;
}
