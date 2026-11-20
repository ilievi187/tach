#pragma once

#include <QStringList>

// Headless commands: `tach status [--json]` and `tach set-level <auto|high|low>`.
// Returns a process exit code. `set-level` is meant to run under pkexec.
int runCli(const QStringList &args);
