#include "cli.h"

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
        out() << QJsonDocument(object).toJson(QJsonDocument::Compact) << "\n";
        return 0;
    }

    out() << backend.cardName() << "  (" << backend.cardPath() << ")\n";
    out() << "  utilisation    " << backend.utilisation() << " %\n";
    out() << "  vram           " << backend.vramUsedGb() << " / " << backend.vramTotalGb() << " GB\n";
    out() << "  temperature    " << backend.edgeTemp() << " / " << backend.junctionTemp() << " / "
          << backend.memTemp() << " C (edge/junction/mem)\n";
    out() << "  power          " << backend.power() << " W\n";
    out() << "  fan            " << backend.fanRpm() << " rpm (max " << backend.fanMax() << ")\n";
    out() << "  clocks         " << backend.coreClock() << " / " << backend.memClock() << " MHz\n";
    out() << "  pcie           " << backend.pcie() << "\n";
    out() << "  performance    " << backend.perfLevel() << "\n";
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

} // namespace

int runCli(const QStringList &args) {
    const QString command = args.value(0);
    if (command == "status")
        return statusCommand(args);
    if (command == "set-level")
        return setLevelCommand(args);
    err() << "usage: tach status [--json] | tach set-level <auto|high|low>\n";
    return 2;
}
