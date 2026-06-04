#!/usr/bin/env bash
set -euo pipefail

CHRONYC_BIN="${CHRONYC_BIN:-chronyc}"
GROUND_TIME_SOURCE="${1:-${GROUND_TIME_SOURCE:-}}"
MAX_OFFSET_MS="${MAX_OFFSET_MS:-2.0}"
MAX_UNCERTAINTY_MS="${MAX_UNCERTAINTY_MS:-2.0}"

run_chronyc() {
  "$CHRONYC_BIN" "$@"
}

abs_le() {
  awk -v value="$1" -v limit="$2" 'BEGIN { if (value < 0) value = -value; exit !(value <= limit) }'
}

le() {
  awk -v value="$1" -v limit="$2" 'BEGIN { exit !(value <= limit) }'
}

source_matches() {
  local selected="$1"
  local expected="$2"
  if [[ -z "$expected" ]]; then
    return 0
  fi
  [[ "$selected" == "$expected" || "$selected" == *"$expected"* || "$expected" == *"$selected"* ]]
}

tracking="$(run_chronyc tracking 2>&1)" || {
  echo "[chrony] FAIL: chronyc tracking failed"
  echo "$tracking"
  exit 1
}
sources="$(run_chronyc sources -v 2>&1)" || {
  echo "[chrony] FAIL: chronyc sources -v failed"
  echo "$sources"
  exit 1
}

offset_ms="$(awk '
  /^System time/ {
    value = $4 * 1000.0
    if ($6 == "slow") value = -value
    printf "%.6f", value
  }' <<<"$tracking")"
root_delay_ms="$(awk '/^Root delay/ { printf "%.6f", $4 * 1000.0 }' <<<"$tracking")"
root_dispersion_ms="$(awk '/^Root dispersion/ { printf "%.6f", $4 * 1000.0 }' <<<"$tracking")"
uncertainty_ms="$(awk -v delay="${root_delay_ms:-0}" -v dispersion="${root_dispersion_ms:-0}" \
  'BEGIN { printf "%.6f", dispersion + delay / 2.0 }')"
leap_status="$(awk -F: '/^Leap status/ { sub(/^[ \t]+/, "", $2); print $2 }' <<<"$tracking")"
selected_source="$(awk '$1 ~ /^[\^=#]\*/ { print $2; exit }' <<<"$sources")"

echo "[chrony] tracking"
echo "$tracking"
echo
echo "[chrony] sources"
echo "$sources"
echo
echo "[chrony] gate"
echo "selected_source=${selected_source:-UNKNOWN}"
echo "ground_time_source=${GROUND_TIME_SOURCE:-ANY}"
echo "offset_ms=${offset_ms:-UNKNOWN} max=${MAX_OFFSET_MS}"
echo "uncertainty_ms=${uncertainty_ms:-UNKNOWN} max=${MAX_UNCERTAINTY_MS}"
echo "leap_status=${leap_status:-UNKNOWN}"
echo "in_flight_step_policy=DISABLED_BY_RUNTIME_SYNC"

if [[ -z "${offset_ms:-}" || -z "${selected_source:-}" || -z "${leap_status:-}" ]]; then
  echo "[chrony] FAIL: missing required chrony fields"
  exit 1
fi
if [[ "$leap_status" != "Normal" ]]; then
  echo "[chrony] FAIL: leap status is not Normal"
  exit 1
fi
if ! source_matches "$selected_source" "$GROUND_TIME_SOURCE"; then
  echo "[chrony] FAIL: selected source is not the configured ground station"
  exit 1
fi
if ! abs_le "$offset_ms" "$MAX_OFFSET_MS"; then
  echo "[chrony] FAIL: offset exceeds preflight gate"
  exit 1
fi
if ! le "$uncertainty_ms" "$MAX_UNCERTAINTY_MS"; then
  echo "[chrony] FAIL: uncertainty exceeds preflight gate"
  exit 1
fi

echo "[chrony] PASS: clock is ready for preflight start gate"
