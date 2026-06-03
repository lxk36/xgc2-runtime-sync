#!/usr/bin/env python3
"""Preview deterministic micro-slot send staggering."""
from __future__ import annotations
import argparse, hashlib


def stable_slot(session: str, channel: str, sender: str, cycle: int, slot_count: int) -> int:
    key = f'{session}|{channel}|{sender}|{cycle}'.encode()
    h = hashlib.blake2b(key, digest_size=8).digest()
    return int.from_bytes(h, 'big') % slot_count


def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument('--session', default='demo')
    p.add_argument('--channel', default='solution')
    p.add_argument('--senders', nargs='+', required=True)
    p.add_argument('--cycle', type=int, default=0)
    p.add_argument('--max-spread-ms', type=float, default=5.0)
    p.add_argument('--slot-width-ms', type=float, default=0.5)
    args = p.parse_args()
    slots = max(1, int(args.max_spread_ms / args.slot_width_ms))
    for s in args.senders:
        slot = stable_slot(args.session, args.channel, s, args.cycle, slots)
        print(f'{s}: slot={slot} offset_ms={slot * args.slot_width_ms:.3f}')


if __name__ == '__main__':
    main()
