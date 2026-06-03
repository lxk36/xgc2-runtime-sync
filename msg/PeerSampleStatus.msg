string peer_id
string channel

uint8 STATUS_FRESH=0
uint8 STATUS_MISSING=1
uint8 STATUS_LATE=2
uint8 STATUS_WRONG_CYCLE=3
uint8 STATUS_DUPLICATE=4
uint8 STATUS_BAD_SCHEMA=5
uint8 STATUS_BAD_PAYLOAD=6
uint8 STATUS_SENDER_CLOCK_BAD=7
uint8 STATUS_SENDER_REPORTED_FAIL=8
uint8 STATUS_TRANSPORT_ERROR=9

uint8 status

uint64 expected_target_cycle
uint64 received_target_cycle
uint64 seq

time local_receive_time
duration receive_latency

bool fresh_for_this_cycle
bool late_for_this_cycle
bool duplicate
bool wrong_cycle
bool bad_schema
bool bad_crc
bool sender_clock_bad

string drop_reason
string schema_id
