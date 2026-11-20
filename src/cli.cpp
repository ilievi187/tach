#include "cli.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include "backend.h"

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
    err() << "usage: tach status [--json] | tach set-level <auto|high|low> | "
             "tach set-profile <index> | tach set-power-cap <watts>\n";
    return 2;
}
