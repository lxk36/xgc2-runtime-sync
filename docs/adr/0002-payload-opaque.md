# ADR 0002: Opaque Payloads

## Status

Accepted for staging.

## Decision

The runtime does not parse business payloads. Payloads are treated as opaque
bytes associated with a schema id and envelope metadata.

## Rationale

Distributed MPC, perception, planning, graph optimization, task allocation, and
consensus algorithms use different payload formats. The runtime should provide
transport, timing, and validity evidence without owning those formats.

## Consequences

- The runtime validates envelope version, schema id, checksum, target cycle,
  sender clock state, and deadlines.
- User code owns payload serialization and deserialization.
- Changing a business schema should not require runtime-core changes.
