#!/usr/bin/env python3
"""Simple startup traffic budget calculator for swarm_sync_runtime_ros1."""
from __future__ import annotations
import argparse


def calc_bps(publishers: int, fanout: int, hz: float, payload: int,
             envelope: int, transport: int, safety: float) -> float:
    return publishers * fanout * hz * (payload + envelope + transport) * 8.0 * safety


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument('--publishers', type=int, required=True)
    p.add_argument('--fanout', type=int, required=True)
    p.add_argument('--hz', type=float, required=True)
    p.add_argument('--payload-bytes', type=int, required=True)
    p.add_argument('--envelope-bytes', type=int, default=128)
    p.add_argument('--transport-bytes', type=int, default=64)
    p.add_argument('--safety', type=float, default=1.35)
    p.add_argument('--budget-bps', type=float, required=True)
    args = p.parse_args()

    bps = calc_bps(args.publishers, args.fanout, args.hz, args.payload_bytes,
                   args.envelope_bytes, args.transport_bytes, args.safety)
    util = bps / args.budget_bps if args.budget_bps else float('inf')
    print(f'estimated_bps: {bps:.0f}')
    print(f'budget_bps: {args.budget_bps:.0f}')
    print(f'utilization: {util:.3f}')
    print('accepted:', 'true' if bps <= args.budget_bps else 'false')


if __name__ == '__main__':
    main()
