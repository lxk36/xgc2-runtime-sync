# Coordinator-level statistics snapshot for synchronized periodic triggering.
time stamp
uint64 sequence_id
float64 sync_period_ms

uint32 expected_response_count
uint32 actual_response_count
uint32 missing_response_count
float64 round_finish_ms

float64 max_cycle_ms
float64 mean_cycle_ms
float64 stddev_cycle_ms
uint32 straggler_participant_id

periodic_sync/SyncParticipantStats[] participants
