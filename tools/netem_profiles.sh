#!/usr/bin/env bash
set -euo pipefail

DEV="${1:-wlan0}"
PROFILE="${2:-help}"

case "$PROFILE" in
  clear)
    sudo tc qdisc del dev "$DEV" root || true
    ;;
  delay20_loss1)
    sudo tc qdisc add dev "$DEV" root netem delay 20ms 5ms loss 1%
    ;;
  delay50_loss5)
    sudo tc qdisc add dev "$DEV" root netem delay 50ms 20ms loss 5%
    ;;
  reorder)
    sudo tc qdisc add dev "$DEV" root netem delay 30ms 10ms reorder 25% 50%
    ;;
  duplicate)
    sudo tc qdisc add dev "$DEV" root netem duplicate 2%
    ;;
  corrupt)
    sudo tc qdisc add dev "$DEV" root netem corrupt 0.1%
    ;;
  help|*)
    echo "Usage: $0 <dev> {clear|delay20_loss1|delay50_loss5|reorder|duplicate|corrupt}"
    ;;
esac
