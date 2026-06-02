# Per-participant statistics reported by the synchronized periodic coordinator.
uint8 STATUS_OK=0
uint8 STATUS_SLOW=1
uint8 STATUS_MISSING=2
uint8 STATUS_FAILED=3
uint8 STATUS_DEGRADED=4

uint32 participant_id
bool ready
string node_name
uint64 last_ack_sequence_id
time last_response_time

float64 last_computation_ms
float64 last_cycle_ms
float64 computation_ewma_ms
float64 cycle_ewma_ms
float64 computation_mean_ms
float64 computation_max_ms
float64 computation_stddev_ms
float64 cycle_mean_ms
float64 cycle_max_ms
float64 cycle_stddev_ms

uint32 missed_count
uint32 consecutive_missed_count
uint32 slow_count
uint32 consecutive_slow_count
uint32 failure_count
uint32 consecutive_failure_count
uint32 good_cycle_count

uint8 status
