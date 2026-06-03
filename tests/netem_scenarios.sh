#!/usr/bin/env bash
set -euo pipefail

DEV=${1:-eth0}
SCENARIO=${2:-clean}

clear_netem() {
  sudo tc qdisc del dev "$DEV" root 2>/dev/null || true
}

apply_clean() {
  clear_netem
  sudo tc qdisc add dev "$DEV" root netem delay 2ms 1ms
}

apply_mild_wifi() {
  clear_netem
  sudo tc qdisc add dev "$DEV" root netem delay 10ms 5ms loss 0.5%
}

apply_bad_wifi() {
  clear_netem
  sudo tc qdisc add dev "$DEV" root netem delay 30ms 20ms loss 3% reorder 1%
}

apply_bandwidth_limited() {
  clear_netem
  sudo tc qdisc add dev "$DEV" root netem delay 20ms 10ms rate 1mbit
}

apply_slot_burst() {
  clear_netem
  sudo tc qdisc add dev "$DEV" root netem slot 5ms 20ms
}

case "$SCENARIO" in
  clear) clear_netem ;;
  clean) apply_clean ;;
  mild_wifi) apply_mild_wifi ;;
  bad_wifi) apply_bad_wifi ;;
  bandwidth_limited) apply_bandwidth_limited ;;
  slot_burst) apply_slot_burst ;;
  *) echo "Unknown scenario: $SCENARIO" >&2; exit 1 ;;
esac

tc qdisc show dev "$DEV"
