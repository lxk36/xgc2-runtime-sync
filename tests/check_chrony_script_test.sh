#!/usr/bin/env bash
set -euo pipefail

SCRIPT_UNDER_TEST="${1:?usage: check_chrony_script_test.sh SCRIPT}"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

FAKE_CHRONYC="$TMPDIR/chronyc"
cat >"$FAKE_CHRONYC" <<'FAKE'
#!/usr/bin/env bash
set -euo pipefail
mode="${FAKE_CHRONY_MODE:-pass}"
if [[ "$1" == "tracking" ]]; then
  offset="0.001000000"
  if [[ "$mode" == "large_offset" ]]; then
    offset="0.004000000"
  fi
  cat <<EOF
Reference ID    : C0A80A0A (192.168.10.10)
System time     : ${offset} seconds fast of NTP time
Root delay      : 0.000200000 seconds
Root dispersion : 0.000100000 seconds
Leap status     : Normal
EOF
elif [[ "$1" == "sources" ]]; then
  cat <<'EOF'
MS Name/IP address         Stratum Poll Reach LastRx Last sample
===============================================================================
^* 192.168.10.10                 1   6   377    12   +1000us[+1000us] +/-  500us
EOF
else
  exit 2
fi
FAKE
chmod +x "$FAKE_CHRONYC"

CHRONYC_BIN="$FAKE_CHRONYC" GROUND_TIME_SOURCE="192.168.10.10" \
  MAX_OFFSET_MS="2.0" MAX_UNCERTAINTY_MS="2.0" "$SCRIPT_UNDER_TEST" >/tmp/check_chrony_pass.log

if CHRONYC_BIN="$FAKE_CHRONYC" FAKE_CHRONY_MODE="large_offset" GROUND_TIME_SOURCE="192.168.10.10" \
  MAX_OFFSET_MS="2.0" MAX_UNCERTAINTY_MS="2.0" "$SCRIPT_UNDER_TEST" >/tmp/check_chrony_fail.log 2>&1; then
  echo "expected large offset to fail"
  exit 1
fi

grep -q "PASS" /tmp/check_chrony_pass.log
grep -q "offset exceeds" /tmp/check_chrony_fail.log
