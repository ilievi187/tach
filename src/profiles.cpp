#include "profiles.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSet>
#include <QThread>

#include <csignal>
#include <cstdio>

#include "backend.h"

namespace {
volatile std::sig_atomic_t gStop = 0;

void requestStop(int) {
    gStop = 1;
}

QString hwmonFor(const QString &card) {
    const QDir dir(card + "/hwmon");
    const auto entries = dir.entryList({"hwmon*"}, QDir::Dirs | QDir::NoDotAndDotDot);
    return entries.isEmpty() ? QString() : card + "/hwmon/" + entries.first();
}

bool writeSysfs(const QString &path, const QString &value) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(value.toUtf8());
    return true;
}

bool isPreset(const QString &name) {
    return name == QLatin1String("quiet") || name == QLatin1String("balanced") ||
           name == QLatin1String("game");
}

void sleepInterruptible(int milliseconds) {
    for (int elapsed = 0; elapsed < milliseconds && !gStop; elapsed += 100)
        QThread::msleep(100);
}
} // namespace

QString Profiles::configPath() {
    return qEnvironmentVariable("TACH_PROFILES_CONFIG", QStringLiteral("/etc/tach/profiles.json"));
}

Profiles::Config Profiles::parseConfig(const QJsonObject &root) {
    Config config;
    config.autoSwitch = root.value("auto").toBool(false);
    const QString fallback = root.value("default").toString("balanced");
    config.fallback = isPreset(fallback) ? fallback : QStringLiteral("balanced");

    for (const auto &entry : root.value("rules").toArray()) {
        const QJsonObject rule = entry.toObject();
        const QString match = rule.value("match").toString().trimmed();
        const QString preset = rule.value("preset").toString();
        if (!match.isEmpty() && isPreset(preset))
            config.rules.append(Rule{match, preset});
    }
    return config;
}

// The process name people know is the executable's basename, not the 15-char
// comm, so prefer the exe link and fall back to comm.
QStringList Profiles::runningProcessNames(const QString &procRoot) {
    QSet<QString> names;
    const QDir proc(procRoot);
    for (const auto &entry : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool numeric = false;
        entry.toInt(&numeric);
        if (!numeric)
            continue;

        QString name = QFileInfo(QString("%1/%2/exe").arg(procRoot, entry)).symLinkTarget();
        if (!name.isEmpty()) {
            name = name.section('/', -1);
            if (name.endsWith(QLatin1String(" (deleted)")))
                name.chop(10);
        }
        if (name.isEmpty()) {
            QFile comm(QString("%1/%2/comm").arg(procRoot, entry));
            if (comm.open(QIODevice::ReadOnly))
                name = QString::fromUtf8(comm.readAll()).trimmed();
        }
        if (!name.isEmpty())
            names.insert(name.toLower());
    }
    return names.values();
}

QString Profiles::choosePreset(const Config &config, const QStringList &names) {
    if (!config.autoSwitch)
        return {};
    for (const auto &rule : config.rules) {
        const QString needle = rule.match.toLower();
        for (const auto &name : names) {
            if (name.contains(needle))
                return rule.preset;
        }
    }
    return config.fallback;
}

int Profiles::applyPreset(const QString &name, Backend &backend) {
    if (!isPreset(name))
        return -1;

    struct Preset {
        QString level;
        QString profile;
        bool capMin, capMax;
        QList<int> percents;
    };
    const QMap<QString, Preset> presets{
        {"quiet", {"auto", "POWER_SAVING", true, false, {0, 20, 40, 70}}},
        {"balanced", {"auto", "BOOTUP_DEFAULT", false, false, {0, 35, 60, 100}}},
        {"game", {"high", "3D_FULL_SCREEN", false, true, {25, 50, 75, 100}}},
    };
    if (!presets.contains(name))
        return -1;
    if (!backend.available())
        return 0;

    const Preset preset = presets.value(name);
    int applied = 0;

    const QString hwmon = hwmonFor(backend.cardPath());
    if (!hwmon.isEmpty()) {
        const double cap = preset.capMin ? backend.powerCapMinW()
                          : preset.capMax ? backend.powerCapMaxW()
                                          : backend.powerCapDefaultW();
        if (cap > 0 && writeSysfs(hwmon + "/power1_cap", QString::number(qRound64(cap * 1e6))))
            ++applied;
    }

    // The profile only takes effect in manual mode on some drivers, so set the
    // level first and fall back to manual if the profile is refused.
    if (writeSysfs(backend.cardPath() + "/power_dpm_force_performance_level", preset.level))
        ++applied;

    QFile table(backend.cardPath() + "/pp_power_profile_mode");
    if (table.open(QIODevice::ReadOnly)) {
        int active = -1;
        const QVariantList profiles =
            Backend::parsePowerProfiles(QString::fromUtf8(table.readAll()), &active);
        table.close();
        for (const auto &entry : profiles) {
            const QVariantMap map = entry.toMap();
            if (!map.value("name").toString().contains(preset.profile))
                continue;

            QFile profileFile(table.fileName());
            if (profileFile.open(QIODevice::WriteOnly)) {
                profileFile.write(QByteArray::number(map.value("index").toInt()));
                profileFile.close();
                ++applied;
            }
            break;
        }
    }

    QJsonArray curve;
    const QList<int> temps{50, 65, 80, 90};
    for (int i = 0; i < temps.size(); ++i)
        curve.append(QJsonObject{{"temp", temps.at(i)}, {"percent", preset.percents.value(i)}});
    const QJsonObject fan{
        {"mode", "curve"},
        {"staticPercent", 50},
        {"tempSource", "junction"},
        {"curve", curve},
    };
    const QString fanPath = Backend::fanConfigPath();
    QDir().mkpath(QFileInfo(fanPath).absolutePath());
    QFile fanFile(fanPath);
    if (fanFile.open(QIODevice::WriteOnly)) {
        fanFile.write(QJsonDocument(fan).toJson(QJsonDocument::Indented));
        fanFile.close();
        ++applied;
        // The fan service re-reads its config every tick, so no restart needed.
        if (qEnvironmentVariableIsEmpty("TACH_NO_SYSTEMD"))
            QProcess::execute("systemctl", {"enable", "tach-fan"});
    }
    return applied;
}

int Profiles::applyConfig(const QString &json) {
    const QJsonObject object = QJsonDocument::fromJson(json.toUtf8()).object();
    if (!object.contains("default")) {
        std::fprintf(stderr, "profiles config needs a default preset\n");
        return 2;
    }

    const QString path = configPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        std::fprintf(stderr, "cannot write %s (run under pkexec)\n", qPrintable(path));
        return 1;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    file.close();

    if (!qEnvironmentVariableIsEmpty("TACH_NO_SYSTEMD")) {
        std::printf("profiles written\n");
        return 0;
    }
    QProcess::execute("systemctl", {"enable", "tach-profiles"});
    if (QProcess::execute("systemctl", {"restart", "tach-profiles"}) != 0) {
        std::fprintf(stderr, "config written, but tach-profiles.service did not start\n");
        return 1;
    }
    std::printf("profiles applied\n");
    return 0;
}

int Profiles::runWatch() {
    Backend backend;
    if (!backend.available()) {
        std::fprintf(stderr, "%s\n", qPrintable(backend.error()));
        return 1;
    }

    std::signal(SIGTERM, requestStop);
    std::signal(SIGINT, requestStop);

    QString current;
    QString pending;
    int pendingTicks = 0;

    while (!gStop) {
        QFile file(configPath());
        Config config;
        if (file.open(QIODevice::ReadOnly))
            config = parseConfig(QJsonDocument::fromJson(file.readAll()).object());

        if (!config.autoSwitch) {
            pending.clear();
            pendingTicks = 0;
            sleepInterruptible(2000);
            continue;
        }

        const QString target = choosePreset(config, runningProcessNames());
        if (target != current) {
            if (target != pending) {
                pending = target;
                pendingTicks = 0;
            }
            // Two consecutive ticks before switching keeps a process that
            // spawns helpers from flapping the card between presets.
            if (++pendingTicks >= 2) {
                const int applied = applyPreset(target, backend);
                if (applied >= 0) {
                    current = target;
                    std::fprintf(stderr, "tach-profiles: %s (%d settings)\n",
                                 qPrintable(target), applied);
                }
                pending.clear();
                pendingTicks = 0;
            }
        } else {
            pending.clear();
            pendingTicks = 0;
        }

        sleepInterruptible(2000);
    }
    return 0;
}
