#!/usr/bin/env bash
set -euo pipefail

echo "[chrony] tracking"
chronyc tracking || true

echo
echo "[chrony] sources"
chronyc sources -v || true

echo
echo "[chrony] waitsync example"
echo "chronyc waitsync 30 0.001"
