# Event emitted by the synchronized periodic coordinator when sync health changes.
uint8 EVENT_READY=0
uint8 EVENT_ALL_READY=1
uint8 EVENT_SLOW=2
uint8 EVENT_MISSED=3
uint8 EVENT_LATE_ACK=4
uint8 EVENT_FAILURE=5
uint8 EVENT_RECOVERED=6
uint8 EVENT_UNKNOWN_PARTICIPANT=7
uint8 EVENT_CLOCK_ANOMALY=8
uint8 EVENT_DEGRADED=9

time stamp
uint8 event_type
uint32 participant_id
uint64 sequence_id
string message

float64 cycle_ms
float64 computation_ms
float64 threshold_ms
