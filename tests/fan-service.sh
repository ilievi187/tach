#!/bin/sh
# Drives the fan service against a fake hwmon tree, so the loop, the curve and
# the restore-on-exit path are exercised without root or a real GPU.
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
BIN="$ROOT/build/tach"
[ -x "$BIN" ] || { echo "build tach first"; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/hwmon"
printf '2\n' > "$TMP/hwmon/pwm1_enable"
printf '0\n' > "$TMP/hwmon/pwm1"
printf '60000\n' > "$TMP/hwmon/temp1_input"
printf '80000\n' > "$TMP/hwmon/temp2_input"   # 80 C junction
printf '1200\n' > "$TMP/hwmon/fan1_input"
printf '3600\n' > "$TMP/hwmon/fan1_max"

cat > "$TMP/fan.json" <<'JSON'
{"mode":"curve","staticPercent":50,"curve":[
  {"temp":50,"percent":0},
  {"temp":65,"percent":35},
  {"temp":80,"percent":60},
  {"temp":90,"percent":100}
]}
JSON

TACH_HWMON="$TMP/hwmon" TACH_FAN_CONFIG="$TMP/fan.json" "$BIN" --fan-service 2>"$TMP/log" &
PID=$!

# 80 C on that curve is 60 % -> 153/255.
for _ in 1 2 3 4 5 6 7 8 9 10; do
    sleep 1
    [ "$(cat "$TMP/hwmon/pwm1_enable")" = "1" ] && break
done

MODE="$(cat "$TMP/hwmon/pwm1_enable")"
PWM="$(cat "$TMP/hwmon/pwm1")"
[ "$MODE" = "1" ] || { echo "FAIL: manual mode not enabled (got $MODE)"; exit 1; }
[ "$PWM" = "153" ] || { echo "FAIL: expected pwm1=153 at 80C/60%, got $PWM"; exit 1; }

# A 0 % curve point is legitimate when cold (zero RPM)...
printf '20000\n' > "$TMP/hwmon/temp2_input"
sleep 3
PWM="$(cat "$TMP/hwmon/pwm1")"
[ "$PWM" -eq 0 ] || { echo "FAIL: expected 0/255 at 20 C with a 0 % point, got $PWM"; exit 1; }

# ...but never when the card is hot, whatever the curve says.
cat > "$TMP/fan.json" <<'JSON'
{"mode":"curve","staticPercent":50,"curve":[
  {"temp":50,"percent":0},
  {"temp":90,"percent":0}
]}
JSON
printf '70000\n' > "$TMP/hwmon/temp2_input"
sleep 3
PWM="$(cat "$TMP/hwmon/pwm1")"
[ "$PWM" -ge 51 ] || { echo "FAIL: 0 % curve stopped the fan at 70 C ($PWM/255)"; exit 1; }

kill -TERM "$PID"
wait "$PID" 2>/dev/null || true
MODE_AFTER="$(cat "$TMP/hwmon/pwm1_enable")"
[ "$MODE_AFTER" = "2" ] || { echo "FAIL: fan was not returned to automatic (got $MODE_AFTER)"; exit 1; }

echo "fan service: ok (153/255 at 80 C, 0 at 20 C cold, clamped to $PWM/255 at 70 C, auto restored on exit)"
