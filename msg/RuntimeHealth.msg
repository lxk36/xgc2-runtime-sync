std_msgs/Header header

string team_id
string session_id
string self_id

bool alive
bool session_running
bool zenoh_connected

bool clock_ok
int64 clock_offset_ns
uint64 clock_uncertainty_ns
uint8 clock_quality

uint64 current_cycle
duration cycle_jitter_p95
duration cycle_jitter_p99

uint32 seq_gap_10s
uint32 late_drop_10s
uint32 missing_10s
uint32 bad_payload_10s

float32 cpu_usage
float32 memory_mb

string state
string reason
