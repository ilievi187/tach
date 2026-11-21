#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

class Backend;

// Automatic profiles: watch what is running and apply a preset when a match
// appears, then go back to the default when it exits.
//
// Every preset write is a root sysfs write, so this runs as a system service
// (tach-profiles.service) instead of asking for a password on every game
// launch. The window only edits /etc/tach/profiles.json through pkexec.
namespace Profiles {

struct Rule {
    QString match;    // case-insensitive substring of a process name
    QString preset;   // quiet | balanced | game
};

struct Config {
    bool autoSwitch = false;
    QString fallback = "balanced";
    QVector<Rule> rules;
};

QString configPath();
Config parseConfig(const QJsonObject &root);
QStringList runningProcessNames(const QString &procRoot = "/proc");
QString choosePreset(const Config &config, const QStringList &names);

// Applies cap, DPM level, power profile and fan curve. Returns the number of
// settings that were written, or -1 when the name is unknown.
int applyPreset(const QString &name, Backend &backend);

int runWatch();                 // --profile-watch, blocks until SIGTERM
int applyConfig(const QString &json);   // runs under pkexec

} // namespace Profiles
