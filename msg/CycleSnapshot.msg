std_msgs/Header header

string team_id
string session_id
string task_id
string self_id

uint64 cycle_id
time cycle_start_time
duration period

PeerSampleStatus[] samples

uint32 fresh_count
uint32 missing_count
uint32 late_count
uint32 wrong_cycle_count
uint32 bad_count

bool all_required_fresh
bool all_required_usable
bool local_clock_ok
bool runtime_degraded
