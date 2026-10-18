#!/usr/bin/env bash
# hardware_test.sh — automated on-target tests for the STM8S103 + DRV8833 firmware.
#
# Covers stages 2–12 of the test plan. Manual-only checks are intentionally
# skipped and listed at the end: 0.4 visual wiring, 1.5 LED, 5.x visual
# rotation, 8.x mechanical stall (nFAULT), 9.2/9.3 power cycle EEPROM.
#
# Requires: i2cdetect, i2ctransfer, and a built motorctl (../util/motorctl).
# Run from anywhere; the paths are resolved relative to this script.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MCTL="$SCRIPT_DIR/../util/motorctl"

BUS=${BUS:-7}
ADDR=${ADDR:-0x60}
I2CDETECT=$(command -v i2cdetect || echo /usr/sbin/i2cdetect)
I2CTRANSFER=$(command -v i2ctransfer || echo /usr/sbin/i2ctransfer)

PASS=0
FAIL=0
RESULTS=""

if [ ! -x "$MCTL" ]; then
    echo "ERROR: $MCTL not found or not executable. Run: make -C \"$SCRIPT_DIR/../util\"" >&2
    exit 2
fi

# --- assertion helpers -------------------------------------------------------

pass() {
    echo "  PASS: $1"
    PASS=$((PASS + 1))
    RESULTS+="PASS $1"$'\n'
}

fail() {
    echo "  FAIL: $1 ${2:-}"
    FAIL=$((FAIL + 1))
    RESULTS+="FAIL $1 ${2:-}"$'\n'
}

# check_contains NAME EXPECTED CMD... — output must contain EXPECTED (regex)
check_contains() {
    local name="$1" expected="$2"; shift 2
    local output
    output=$("$@" 2>&1) || true
    if grep -q "$expected" <<<"$output"; then
        pass "$name"
    else
        fail "$name" "(expected '$expected', got '$output')"
    fi
}

# check_silent NAME CMD... — command must exit 0 (i2ctransfer success = no I2C error)
check_silent() {
    local name="$1"; shift
    if "$@" >/dev/null 2>&1; then
        pass "$name"
    else
        fail "$name" "(command failed: $*)"
    fi
}

# check_no_i2c_error NAME CMD... — for fire-and-forget frames the firmware should
# ACK but silently ignore: assert no "Error" in output AND exit 0.
check_no_i2c_error() {
    local name="$1"; shift
    local output rc
    output=$("$@" 2>&1) && rc=0 || rc=$?
    if [ "$rc" -eq 0 ] && ! grep -qi "error" <<<"$output"; then
        pass "$name"
    else
        fail "$name" "(rc=$rc, out='$output')"
    fi
}

# Focus/zoom position as an integer, robust to the "focus motor position: N" text.
get_pos() {
    "$MCTL" position "$1" 2>/dev/null | grep -oE '\-?[0-9]+' | head -1 || true
}

# check_pos NAME EXPECTED MOTOR — numeric compare with a 1-step tolerance
check_pos() {
    local name="$1" expected="$2" motor="$3"
    local got delta
    got=$(get_pos "$motor")
    delta=$(( got - expected )); delta=${delta#-}
    if [ -n "$got" ] && [ "$delta" -le 1 ]; then
        pass "$name"
    else
        fail "$name" "(expected $expected, got '${got:-<empty>}')"
    fi
}

reset_mcu() { "$I2CTRANSFER" -y "$BUS" "w1@$ADDR" 0x06 >/dev/null 2>&1 || true; sleep 1; }

echo "=== Stage 2: I2C communication ==="

# 2.1 Detection: the 0x60 row must list the address itself, not only the row label.
# Capture the scan first: piping i2cdetect straight into `grep -q` lets grep exit
# after the first match, so i2cdetect dies with SIGPIPE (141); under `set -o
# pipefail` that non-zero status is misread as "device absent".
DETECT_OUT=$("$I2CDETECT" -y "$BUS" 2>/dev/null || true)
if grep -qE '^60:[[:space:]]+60' <<<"$DETECT_OUT"; then
    pass "2.1 I2C detect 0x60"
else
    fail "2.1 I2C detect 0x60" "(address not present on bus $BUS)"
fi
check_contains "2.2 Firmware version (motorctl)" "2.0.0" "$MCTL" version
check_contains "2.3 Raw version" "0x02 0x00 0x00" "$I2CTRANSFER" -y "$BUS" "w1@$ADDR" 0xFF r3

echo "=== Stage 3: Focus motor ==="
reset_mcu
check_contains "3.1 Focus status" "IDLE" "$MCTL" status focus
BASE_FOCUS=$(get_pos focus); BASE_FOCUS=${BASE_FOCUS:-0}
echo "  (focus baseline = $BASE_FOCUS)"

check_contains "3.2 Focus speed 500" "Speed set to 500" "$MCTL" speed focus 500
check_no_i2c_error "3.2 Speed=0 clamp (no I2C error)" "$I2CTRANSFER" -y "$BUS" "w4@$ADDR" 0x02 0x00 0x00 0x00
check_no_i2c_error "3.2 Speed>5000 clamp (no I2C error)" "$I2CTRANSFER" -y "$BUS" "w4@$ADDR" 0x02 0x00 0x27 0x10
check_contains "3.2 Focus speed back to 500" "Speed set to 500" "$MCTL" speed focus 500

"$MCTL" move focus 200 >/dev/null 2>&1
sleep 1
check_pos "3.4 Pos after +200" "$((BASE_FOCUS + 200))" focus

"$MCTL" move focus -200 >/dev/null 2>&1
sleep 1
check_pos "3.5 Pos after -200" "$BASE_FOCUS" focus

# 3.6 Move rejected while speed==0: reset clears speed, so the move must be a no-op.
reset_mcu
P0=$(get_pos focus)
"$MCTL" move focus 100 >/dev/null 2>&1
sleep 1
P1=$(get_pos focus)
if [ -n "$P0" ] && [ "$P0" = "$P1" ]; then pass "3.6 Move without speed is a no-op"; else fail "3.6 Move without speed is a no-op" "($P0 -> $P1)"; fi

echo "=== Stage 4: Zoom motor ==="
reset_mcu
"$MCTL" speed zoom 500 >/dev/null 2>&1
check_contains "4.1 Zoom status" "IDLE" "$MCTL" status zoom
BASE_ZOOM=$(get_pos zoom); BASE_ZOOM=${BASE_ZOOM:-0}
echo "  (zoom baseline = $BASE_ZOOM)"

"$MCTL" move zoom 200 >/dev/null 2>&1
sleep 1
check_pos "4.1 Zoom +200" "$((BASE_ZOOM + 200))" zoom

"$MCTL" move zoom -200 >/dev/null 2>&1
sleep 1
check_pos "4.1 Zoom -200" "$BASE_ZOOM" zoom

# 4.2 Move rejected while speed==0.
reset_mcu
P0=$(get_pos zoom)
"$MCTL" move zoom 100 >/dev/null 2>&1
sleep 1
P1=$(get_pos zoom)
if [ -n "$P0" ] && [ "$P0" = "$P1" ]; then pass "4.2 Zoom move without speed is a no-op"; else fail "4.2 Zoom move without speed is a no-op" "($P0 -> $P1)"; fi

echo "=== Stage 5: Simultaneous operation ==="
reset_mcu
"$MCTL" speed focus 300 >/dev/null 2>&1
"$MCTL" speed zoom 300 >/dev/null 2>&1
"$MCTL" move focus 300 >/dev/null 2>&1 &
"$MCTL" move zoom 300 >/dev/null 2>&1 &
wait
sleep 2
check_contains "5.1 Focus IDLE after dual move" "IDLE" "$MCTL" status focus
check_contains "5.1 Zoom IDLE after dual move" "IDLE" "$MCTL" status zoom
# No nFAULT/over-current during simultaneous motion is the real criterion:
# both motors must be IDLE (not ERROR) after the dual move.

echo "=== Stage 7: Boundary cases ==="
"$MCTL" speed focus 200 >/dev/null 2>&1
P0=$(get_pos focus)
"$MCTL" move focus 0 >/dev/null 2>&1
sleep 0.5
P1=$(get_pos focus)
if [ -n "$P0" ] && [ "$P0" = "$P1" ]; then pass "7.1 Move 0 steps is a no-op"; else fail "7.1 Move 0 steps is a no-op" "($P0 -> $P1)"; fi

# 7.2 int16 min: accept the command, start, then stop.
"$MCTL" speed focus 200 >/dev/null 2>&1
"$MCTL" move focus -32768 >/dev/null 2>&1
sleep 2
check_contains "7.2 Move -32768 starts" "RUNNING" "$MCTL" status focus
"$MCTL" stop focus >/dev/null 2>&1
sleep 1
check_contains "7.2 Stop after -32768" "IDLE" "$MCTL" status focus

# 7.3 int16 max.
"$MCTL" speed focus 200 >/dev/null 2>&1
"$MCTL" move focus 32767 >/dev/null 2>&1
sleep 2
check_contains "7.3 Move +32767 starts" "RUNNING" "$MCTL" status focus
"$MCTL" stop focus >/dev/null 2>&1
sleep 1
check_contains "7.3 Stop after +32767" "IDLE" "$MCTL" status focus

check_no_i2c_error "7.4 Invalid motor ID ignored" "$I2CTRANSFER" -y "$BUS" "w4@$ADDR" 0x01 0xFF 0x00 0x64
check_no_i2c_error "7.5 Unknown command ignored" "$I2CTRANSFER" -y "$BUS" "w2@$ADDR" 0x08 0x00
check_no_i2c_error "7.6 Short frame ignored" "$I2CTRANSFER" -y "$BUS" "w2@$ADDR" 0x01 0x00

echo "=== Stage 10: I2C stability ==="
fail_count=0
for _ in $(seq 1 100); do
    "$MCTL" version >/dev/null 2>&1 || fail_count=$((fail_count + 1))
done
if [ "$fail_count" -eq 0 ]; then pass "10.1 100x version (0 fails)"; else fail "10.1 100x version ($fail_count fails)"; fi

sleep 3   # > I2C_TIMEOUT_MS (2000 ms)
check_contains "10.2 I2C timeout recovery" "2.0.0" "$MCTL" version

for i in $(seq 1 50); do
    "$MCTL" speed focus $(( (i % 10) * 100 + 100 )) >/dev/null 2>&1
    "$MCTL" move focus 10 >/dev/null 2>&1
    sleep 0.05
done
sleep 2
check_contains "10.3 Focus IDLE after stress" "IDLE" "$MCTL" status focus
check_contains "10.3 Zoom IDLE after stress" "IDLE" "$MCTL" status zoom

echo "=== Stage 11: Watchdog ==="
P0=$(get_pos focus)
sleep 30
P1=$(get_pos focus)
if [ -n "$P0" ] && [ "$P0" = "$P1" ]; then pass "11.1 WDT no reset (position stable)"; else fail "11.1 WDT reset detected" "($P0 -> $P1)"; fi

echo "=== Stage 12: Reset command ==="
reset_mcu
"$MCTL" speed focus 200 >/dev/null 2>&1
B12=$(get_pos focus)
"$MCTL" move focus 100 >/dev/null 2>&1
sleep 2
check_pos "12.1 Pos before reset" "$((B12 + 100))" focus

"$MCTL" reset >/dev/null 2>&1
sleep 1
check_contains "12.1 Controller alive after reset" "2.0.0" "$MCTL" version

echo "=== SUMMARY ==="
echo "PASS: $PASS"
echo "FAIL: $FAIL"
echo "$RESULTS"
echo
echo "Manual checks still required (not automated):"
echo "  0.4 visual wiring, 1.5 LED blink, 5.x visual rotation,"
echo "  7.2/7.3 direction sanity, 8.x nFAULT stall, 9.2/9.3 power-cycle EEPROM."
exit "$FAIL"