#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <vector>

#include <boost/bind.hpp>
#include <ros/ros.h>

#include "periodic_sync/SyncAck.h"
#include "periodic_sync/SyncEvent.h"
#include "periodic_sync/SyncParticipantStats.h"
#include "periodic_sync/SyncReady.h"
#include "periodic_sync/SyncStatistics.h"
#include "periodic_sync/SyncTrigger.h"

class SyncCoordinator {
public:
    explicit SyncCoordinator(ros::NodeHandle& nh) : nh_(nh), sequence_id_(0), all_ready_(false) {
        ros::NodeHandle private_nh("~");
        participant_count_ = private_nh.param("participant_count", 0);
        if (participant_count_ <= 0) {
            participant_count_ = private_nh.param("num_participants", 0);
        }
        if (participant_count_ <= 0) {
            participant_count_ = private_nh.param("num_uavs", 4);
        }
        first_participant_id_ = private_nh.param("first_participant_id", 1);
        const double sampling_time = private_nh.param("sampling_time", 0.1);
        sync_period_ = private_nh.param("sync_period", sampling_time);
        if (private_nh.hasParam("sync_period") &&
            private_nh.hasParam("sampling_time") &&
            std::isfinite(sync_period_) &&
            std::isfinite(sampling_time) &&
            std::abs(sync_period_ - sampling_time) > 1e-9) {
            ROS_WARN("sync_period=%.3f s differs from sampling_time=%.3f s",
                     sync_period_,
                     sampling_time);
        }
        if (!std::isfinite(sync_period_) || sync_period_ <= 0.0) {
            const double fallback_period =
                std::isfinite(sampling_time) && sampling_time > 0.0 ? sampling_time : 0.1;
            ROS_WARN("Invalid sync_period=%.3f s; using %.3f s",
                     sync_period_,
                     fallback_period);
            sync_period_ = fallback_period;
        }
        timeout_threshold_ms_ = private_nh.param("timeout_threshold_ms", 100.0);
        max_consecutive_timeouts_ = private_nh.param("max_consecutive_timeouts", 3);
        ready_topic_ = private_nh.param<std::string>("ready_topic", "/sync/ready");
        trigger_topic_ = private_nh.param<std::string>("trigger_topic", "/sync/trigger");
        ack_topic_ = private_nh.param<std::string>("ack_topic", "/sync/ack");
        statistics_topic_ = private_nh.param<std::string>("statistics_topic", "/sync/statistics");
        events_topic_ = private_nh.param<std::string>("events_topic", "/sync/events");
        statistics_publish_period_ = private_nh.param("statistics_publish_period", 1.0);
        statistics_publish_every_n_cycles_ = private_nh.param("statistics_publish_every_n_cycles", 0);
        stats_window_size_ = std::max(1, private_nh.param("stats_window_size", 100));
        slow_ratio_threshold_ = private_nh.param("slow_ratio_threshold", 0.8);
        recovery_good_cycles_ = std::max(1, private_nh.param("recovery_good_cycles", 5));
        spin_rate_hz_ = private_nh.param("spin_rate_hz", 200.0);
        if (!std::isfinite(spin_rate_hz_) || spin_rate_hz_ <= 0.0) {
            spin_rate_hz_ = 200.0;
        }
        const int sync_queue_size =
            std::max(50, private_nh.param("sync_queue_size", participant_count_ * 4));

        ROS_INFO("========== Periodic Sync Coordinator ==========");
        ROS_INFO("Participants: %d", participant_count_);
        ROS_INFO("Participant ID range: %d..%d",
                 first_participant_id_,
                 first_participant_id_ + std::max(0, participant_count_ - 1));
        ROS_INFO("Sync period: %.3f s", sync_period_);
        ROS_INFO("Timeout threshold: %.1f ms", timeout_threshold_ms_);
        ROS_INFO("Ready/trigger/ack topics: %s | %s | %s",
                 ready_topic_.c_str(),
                 trigger_topic_.c_str(),
                 ack_topic_.c_str());
        ROS_INFO("Statistics/events topics: %s | %s",
                 statistics_topic_.c_str(),
                 events_topic_.c_str());
        ROS_INFO("Spin rate: %.1f Hz, sync queue size: %d",
                 spin_rate_hz_,
                 sync_queue_size);
        ROS_INFO("===============================================");

        for (int offset = 0; offset < participant_count_; ++offset) {
            const uint32_t participant_id = static_cast<uint32_t>(first_participant_id_ + offset);
            participant_status_[participant_id] = ParticipantStatus();
        }

        sync_trigger_pub_ = nh_.advertise<periodic_sync::SyncTrigger>(trigger_topic_, 10);
        statistics_pub_ = nh_.advertise<periodic_sync::SyncStatistics>(statistics_topic_, 10, true);
        event_pub_ = nh_.advertise<periodic_sync::SyncEvent>(events_topic_, 50);
        ready_sub_ = nh_.subscribe(ready_topic_, sync_queue_size, &SyncCoordinator::readyCallback, this);
        sync_ack_sub_ = nh_.subscribe(ack_topic_, sync_queue_size, &SyncCoordinator::syncAckCallback, this);

        if (statistics_publish_period_ > 0.0) {
            statistics_timer_ = nh_.createTimer(
                ros::Duration(statistics_publish_period_),
                &SyncCoordinator::statisticsTimerCallback,
                this);
        }

        ROS_INFO("Waiting for %d participants to be ready...", participant_count_);
    }

    void spin() {
        ros::Rate rate(spin_rate_hz_);
        while (ros::ok()) {
            ros::spinOnce();

            if (!all_ready_ && checkAllReady()) {
                all_ready_ = true;
                ROS_INFO("All %d participants ready. Starting synchronized periodic triggers.",
                         participant_count_);
                publishEvent(periodic_sync::SyncEvent::EVENT_ALL_READY,
                             0,
                             sequence_id_,
                             "All participants are ready",
                             0.0,
                             0.0,
                             0.0);
                publishStatistics();
                startSyncTimer();
            }

            rate.sleep();
        }
    }

private:
    struct TimingSample {
        double computation_ms = 0.0;
        double cycle_ms = 0.0;
    };

    struct ParticipantStatus {
        bool ready = false;
        std::string node_name;
        ros::Time last_response_time;
        uint64_t last_ack_sequence_id = 0;
        double last_computation_ms = 0.0;
        double last_cycle_ms = 0.0;
        double computation_ewma_ms = 0.0;
        double cycle_ewma_ms = 0.0;
        bool has_sample = false;
        std::deque<TimingSample> samples;
        uint32_t missed_count = 0;
        uint32_t consecutive_missed_count = 0;
        uint32_t slow_count = 0;
        uint32_t consecutive_slow_count = 0;
        uint32_t failure_count = 0;
        uint32_t consecutive_failure_count = 0;
        uint32_t good_cycle_count = 0;
        uint8_t status = periodic_sync::SyncParticipantStats::STATUS_OK;
    };

    struct RoundStatus {
        ros::Time trigger_time;
        std::set<uint32_t> pending;
        std::set<uint32_t> responded;
        double round_finish_ms = 0.0;
        uint32_t straggler_participant_id = 0;
        bool monitor_checked = false;
    };

    struct SeriesStats {
        double mean = 0.0;
        double max = 0.0;
        double stddev = 0.0;
    };

    void readyCallback(const periodic_sync::SyncReady::ConstPtr& msg) {
        auto it = participant_status_.find(msg->participant_id);
        if (it == participant_status_.end()) {
            publishEvent(periodic_sync::SyncEvent::EVENT_UNKNOWN_PARTICIPANT,
                         msg->participant_id,
                         sequence_id_,
                         "Ignoring ready message from unexpected participant",
                         0.0,
                         0.0,
                         0.0);
            ROS_WARN_THROTTLE(5.0,
                              "Ignoring ready message from unexpected participant %u",
                              msg->participant_id);
            return;
        }

        ParticipantStatus& status = it->second;
        if (!status.ready) {
            status.ready = true;
            status.node_name = msg->node_name;
            status.last_response_time = msg->ready_time;
            ROS_INFO("Participant %u (%s) is ready",
                     msg->participant_id,
                     msg->node_name.c_str());
            publishEvent(periodic_sync::SyncEvent::EVENT_READY,
                         msg->participant_id,
                         sequence_id_,
                         "Participant is ready",
                         0.0,
                         0.0,
                         0.0);
            publishStatistics();
        }
    }

    void syncAckCallback(const periodic_sync::SyncAck::ConstPtr& msg) {
        auto participant_it = participant_status_.find(msg->participant_id);
        if (participant_it == participant_status_.end()) {
            publishEvent(periodic_sync::SyncEvent::EVENT_UNKNOWN_PARTICIPANT,
                         msg->participant_id,
                         msg->sequence_id,
                         "Ignoring ack from unexpected participant",
                         0.0,
                         msg->computation_ms,
                         0.0);
            ROS_WARN_THROTTLE(5.0,
                              "Ignoring ack from unexpected participant %u",
                              msg->participant_id);
            return;
        }

        RoundStatus* round = nullptr;
        auto round_it = rounds_.find(msg->sequence_id);
        if (round_it != rounds_.end()) {
            round = &round_it->second;
        }

        const ros::Time ack_time = ros::Time::now();
        bool late_ack = false;
        if (round) {
            late_ack = round->monitor_checked && round->pending.count(msg->participant_id) > 0;
            round->pending.erase(msg->participant_id);
            round->responded.insert(msg->participant_id);

            const double ack_delay_ms =
                (ack_time - round->trigger_time).toSec() * 1000.0;
            if (ack_delay_ms >= round->round_finish_ms) {
                round->round_finish_ms = ack_delay_ms;
                round->straggler_participant_id = msg->participant_id;
            }
        }

        ParticipantStatus& status = participant_it->second;
        const uint8_t previous_status = status.status;
        status.last_response_time = ros::Time::now();
        status.last_ack_sequence_id = msg->sequence_id;

        const double computation_ms = msg->computation_ms;
        const double cycle_ms = (msg->finished_time - msg->received_time).toSec() * 1000.0;
        const bool clock_anomaly = cycle_ms < 0.0 || !std::isfinite(cycle_ms);
        const bool slow = !clock_anomaly && cycle_ms > slowThresholdMs();
        const bool failed = !msg->success;
        bool failure_recorded = false;

        if (clock_anomaly) {
            recordFailure(status);
            failure_recorded = true;
            setStatus(status, degradedIfNeeded(status, periodic_sync::SyncParticipantStats::STATUS_FAILED));
            publishEvent(periodic_sync::SyncEvent::EVENT_CLOCK_ANOMALY,
                         msg->participant_id,
                         msg->sequence_id,
                         "Participant reported invalid cycle timestamps",
                         cycle_ms,
                         computation_ms,
                         0.0);
        } else {
            recordSample(status, computation_ms, cycle_ms);
        }

        if (late_ack) {
            publishEvent(periodic_sync::SyncEvent::EVENT_LATE_ACK,
                         msg->participant_id,
                         msg->sequence_id,
                         "Ack arrived after the response timeout",
                         clock_anomaly ? 0.0 : cycle_ms,
                         computation_ms,
                         timeout_threshold_ms_);
        }

        if (failed) {
            if (!failure_recorded) {
                recordFailure(status);
            }
            setStatus(status, degradedIfNeeded(status, periodic_sync::SyncParticipantStats::STATUS_FAILED));
            publishEvent(periodic_sync::SyncEvent::EVENT_FAILURE,
                         msg->participant_id,
                         msg->sequence_id,
                         msg->error_msg.empty() ? "Participant reported failure" : msg->error_msg,
                         clock_anomaly ? 0.0 : cycle_ms,
                         computation_ms,
                         0.0);
        }

        if (slow) {
            ++status.slow_count;
            ++status.consecutive_slow_count;
            status.consecutive_missed_count = 0;
            status.good_cycle_count = 0;
            setStatus(status, degradedIfNeeded(status, periodic_sync::SyncParticipantStats::STATUS_SLOW));
            publishEvent(periodic_sync::SyncEvent::EVENT_SLOW,
                         msg->participant_id,
                         msg->sequence_id,
                         "Participant cycle time exceeded the slow threshold",
                         cycle_ms,
                         computation_ms,
                         slowThresholdMs());
        }

        if (!clock_anomaly && !failed && !slow) {
            recordHealthyAck(status, msg->participant_id, msg->sequence_id, cycle_ms, computation_ms);
        } else if (previous_status != periodic_sync::SyncParticipantStats::STATUS_DEGRADED &&
                   status.status == periodic_sync::SyncParticipantStats::STATUS_DEGRADED) {
            publishEvent(periodic_sync::SyncEvent::EVENT_DEGRADED,
                         msg->participant_id,
                         msg->sequence_id,
                         "Participant entered degraded status",
                         clock_anomaly ? 0.0 : cycle_ms,
                         computation_ms,
                         timeout_threshold_ms_);
        }

        publishStatisticsIfCycleDue();
        cleanupRounds();
    }

    bool checkAllReady() const {
        for (const auto& pair : participant_status_) {
            if (!pair.second.ready) {
                return false;
            }
        }
        return !participant_status_.empty();
    }

    void startSyncTimer() {
        sync_timer_ = nh_.createTimer(
            ros::Duration(sync_period_),
            &SyncCoordinator::syncTimerCallback,
            this);
    }

    void syncTimerCallback(const ros::TimerEvent&) {
        periodic_sync::SyncTrigger trigger;
        trigger.sequence_id = ++sequence_id_;
        trigger.trigger_time = ros::Time::now();

        RoundStatus round;
        round.trigger_time = trigger.trigger_time;
        for (const auto& pair : participant_status_) {
            trigger.active_participant_ids.push_back(pair.first);
            round.pending.insert(pair.first);
        }
        rounds_[trigger.sequence_id] = round;

        sync_trigger_pub_.publish(trigger);

        response_timers_[trigger.sequence_id] = nh_.createTimer(
            ros::Duration(timeout_threshold_ms_ / 1000.0),
            boost::bind(&SyncCoordinator::checkResponses, this, trigger.sequence_id),
            true);

        publishStatisticsIfCycleDue();
        cleanupRounds();
    }

    void checkResponses(uint64_t sequence_id) {
        auto round_it = rounds_.find(sequence_id);
        if (round_it != rounds_.end()) {
            RoundStatus& round = round_it->second;
            round.monitor_checked = true;
            for (const uint32_t participant_id : round.pending) {
                ParticipantStatus& status = participant_status_[participant_id];
                const uint8_t previous_status = status.status;
                ++status.missed_count;
                ++status.consecutive_missed_count;
                status.good_cycle_count = 0;
                setStatus(status, degradedIfNeeded(status, periodic_sync::SyncParticipantStats::STATUS_MISSING));
                publishEvent(periodic_sync::SyncEvent::EVENT_MISSED,
                             participant_id,
                             sequence_id,
                             "Participant did not ack before the response timeout",
                             0.0,
                             0.0,
                             timeout_threshold_ms_);

                ROS_WARN("[Seq %lu] Participant %u NO RESPONSE (timeouts: %u)",
                         sequence_id,
                         participant_id,
                         status.consecutive_missed_count);

                if (previous_status != periodic_sync::SyncParticipantStats::STATUS_DEGRADED &&
                    status.status == periodic_sync::SyncParticipantStats::STATUS_DEGRADED) {
                    publishEvent(periodic_sync::SyncEvent::EVENT_DEGRADED,
                                 participant_id,
                                 sequence_id,
                                 "Participant entered degraded status",
                                 0.0,
                                 0.0,
                                 timeout_threshold_ms_);
                    ROS_ERROR("Participant %u entered DEGRADED mode", participant_id);
                }
            }
        }

        response_timers_.erase(sequence_id);
        publishStatistics();
        cleanupRounds();
    }

    void statisticsTimerCallback(const ros::TimerEvent&) {
        publishStatistics();
    }

    void publishStatisticsIfCycleDue() {
        if (statistics_publish_every_n_cycles_ > 0 &&
            sequence_id_ > 0 &&
            sequence_id_ % static_cast<uint64_t>(statistics_publish_every_n_cycles_) == 0) {
            publishStatistics();
        }
    }

    void publishStatistics() {
        periodic_sync::SyncStatistics msg;
        msg.stamp = ros::Time::now();
        msg.sequence_id = sequence_id_;
        msg.sync_period_ms = sync_period_ * 1000.0;

        const RoundStatus* round = latestRound();
        msg.expected_response_count = static_cast<uint32_t>(participant_status_.size());
        if (round) {
            msg.actual_response_count = static_cast<uint32_t>(round->responded.size());
            msg.missing_response_count = static_cast<uint32_t>(round->pending.size());
            msg.round_finish_ms = round->round_finish_ms;
            msg.straggler_participant_id = round->straggler_participant_id;
        } else {
            msg.actual_response_count = 0;
            msg.missing_response_count = msg.expected_response_count;
            msg.round_finish_ms = 0.0;
            msg.straggler_participant_id = 0;
        }

        std::vector<double> latest_cycle_samples;
        latest_cycle_samples.reserve(participant_status_.size());
        for (const auto& pair : participant_status_) {
            msg.participants.push_back(toParticipantStats(pair.first, pair.second));
            if (pair.second.has_sample) {
                latest_cycle_samples.push_back(pair.second.last_cycle_ms);
            }
        }

        const SeriesStats global_cycle_stats = computeStats(latest_cycle_samples);
        msg.max_cycle_ms = global_cycle_stats.max;
        msg.mean_cycle_ms = global_cycle_stats.mean;
        msg.stddev_cycle_ms = global_cycle_stats.stddev;

        statistics_pub_.publish(msg);
    }

    void publishEvent(uint8_t event_type,
                      uint32_t participant_id,
                      uint64_t sequence_id,
                      const std::string& text,
                      double cycle_ms,
                      double computation_ms,
                      double threshold_ms) {
        periodic_sync::SyncEvent event;
        event.stamp = ros::Time::now();
        event.event_type = event_type;
        event.participant_id = participant_id;
        event.sequence_id = sequence_id;
        event.message = text;
        event.cycle_ms = cycle_ms;
        event.computation_ms = computation_ms;
        event.threshold_ms = threshold_ms;
        event_pub_.publish(event);
    }

    void recordSample(ParticipantStatus& status, double computation_ms, double cycle_ms) {
        status.last_computation_ms = computation_ms;
        status.last_cycle_ms = cycle_ms;
        if (!status.has_sample) {
            status.computation_ewma_ms = computation_ms;
            status.cycle_ewma_ms = cycle_ms;
            status.has_sample = true;
        } else {
            constexpr double ewma_alpha = 0.05;
            status.computation_ewma_ms =
                status.computation_ewma_ms * (1.0 - ewma_alpha) + computation_ms * ewma_alpha;
            status.cycle_ewma_ms =
                status.cycle_ewma_ms * (1.0 - ewma_alpha) + cycle_ms * ewma_alpha;
        }

        status.samples.push_back(TimingSample{computation_ms, cycle_ms});
        while (static_cast<int>(status.samples.size()) > stats_window_size_) {
            status.samples.pop_front();
        }
    }

    void recordFailure(ParticipantStatus& status) {
        ++status.failure_count;
        ++status.consecutive_failure_count;
        status.good_cycle_count = 0;
    }

    void recordHealthyAck(ParticipantStatus& status,
                          uint32_t participant_id,
                          uint64_t sequence_id,
                          double cycle_ms,
                          double computation_ms) {
        const bool was_abnormal = status.status != periodic_sync::SyncParticipantStats::STATUS_OK;
        status.consecutive_missed_count = 0;
        status.consecutive_slow_count = 0;
        status.consecutive_failure_count = 0;
        ++status.good_cycle_count;

        if (was_abnormal) {
            if (status.good_cycle_count >= static_cast<uint32_t>(recovery_good_cycles_)) {
                status.status = periodic_sync::SyncParticipantStats::STATUS_OK;
                publishEvent(periodic_sync::SyncEvent::EVENT_RECOVERED,
                             participant_id,
                             sequence_id,
                             "Participant recovered after consecutive healthy acks",
                             cycle_ms,
                             computation_ms,
                             0.0);
            }
        } else {
            status.status = periodic_sync::SyncParticipantStats::STATUS_OK;
        }
    }

    uint8_t degradedIfNeeded(const ParticipantStatus& status, uint8_t fallback_status) const {
        if (status.consecutive_missed_count >= static_cast<uint32_t>(max_consecutive_timeouts_) ||
            status.consecutive_slow_count >= static_cast<uint32_t>(max_consecutive_timeouts_) ||
            status.consecutive_failure_count >= static_cast<uint32_t>(max_consecutive_timeouts_)) {
            return periodic_sync::SyncParticipantStats::STATUS_DEGRADED;
        }
        return fallback_status;
    }

    void setStatus(ParticipantStatus& status, uint8_t candidate_status) const {
        if (statusRank(candidate_status) >= statusRank(status.status)) {
            status.status = candidate_status;
        }
    }

    static int statusRank(uint8_t status) {
        switch (status) {
            case periodic_sync::SyncParticipantStats::STATUS_OK:
                return 0;
            case periodic_sync::SyncParticipantStats::STATUS_SLOW:
                return 1;
            case periodic_sync::SyncParticipantStats::STATUS_MISSING:
                return 2;
            case periodic_sync::SyncParticipantStats::STATUS_FAILED:
                return 3;
            case periodic_sync::SyncParticipantStats::STATUS_DEGRADED:
                return 4;
            default:
                return 0;
        }
    }

    periodic_sync::SyncParticipantStats toParticipantStats(
        uint32_t participant_id,
        const ParticipantStatus& status) const {
        periodic_sync::SyncParticipantStats msg;
        msg.participant_id = participant_id;
        msg.ready = status.ready;
        msg.node_name = status.node_name;
        msg.last_ack_sequence_id = status.last_ack_sequence_id;
        msg.last_response_time = status.last_response_time;
        msg.last_computation_ms = status.last_computation_ms;
        msg.last_cycle_ms = status.last_cycle_ms;
        msg.computation_ewma_ms = status.computation_ewma_ms;
        msg.cycle_ewma_ms = status.cycle_ewma_ms;

        std::vector<double> computation_samples;
        std::vector<double> cycle_samples;
        computation_samples.reserve(status.samples.size());
        cycle_samples.reserve(status.samples.size());
        for (const TimingSample& sample : status.samples) {
            computation_samples.push_back(sample.computation_ms);
            cycle_samples.push_back(sample.cycle_ms);
        }

        const SeriesStats computation_stats = computeStats(computation_samples);
        const SeriesStats cycle_stats = computeStats(cycle_samples);
        msg.computation_mean_ms = computation_stats.mean;
        msg.computation_max_ms = computation_stats.max;
        msg.computation_stddev_ms = computation_stats.stddev;
        msg.cycle_mean_ms = cycle_stats.mean;
        msg.cycle_max_ms = cycle_stats.max;
        msg.cycle_stddev_ms = cycle_stats.stddev;

        msg.missed_count = status.missed_count;
        msg.consecutive_missed_count = status.consecutive_missed_count;
        msg.slow_count = status.slow_count;
        msg.consecutive_slow_count = status.consecutive_slow_count;
        msg.failure_count = status.failure_count;
        msg.consecutive_failure_count = status.consecutive_failure_count;
        msg.good_cycle_count = status.good_cycle_count;
        msg.status = status.status;
        return msg;
    }

    static SeriesStats computeStats(const std::vector<double>& values) {
        SeriesStats stats;
        if (values.empty()) {
            return stats;
        }

        stats.max = *std::max_element(values.begin(), values.end());
        const double sum = std::accumulate(values.begin(), values.end(), 0.0);
        stats.mean = sum / static_cast<double>(values.size());

        double squared_error_sum = 0.0;
        for (const double value : values) {
            const double error = value - stats.mean;
            squared_error_sum += error * error;
        }
        stats.stddev = std::sqrt(squared_error_sum / static_cast<double>(values.size()));
        return stats;
    }

    const RoundStatus* latestRound() const {
        if (rounds_.empty()) {
            return nullptr;
        }
        return &rounds_.rbegin()->second;
    }

    double slowThresholdMs() const {
        return sync_period_ * 1000.0 * slow_ratio_threshold_;
    }

    void cleanupRounds() {
        const uint64_t keep_count = static_cast<uint64_t>(std::max(stats_window_size_, 10));
        while (!rounds_.empty() && rounds_.begin()->first + keep_count < sequence_id_) {
            rounds_.erase(rounds_.begin());
        }
    }

    ros::NodeHandle nh_;
    ros::Publisher sync_trigger_pub_;
    ros::Publisher statistics_pub_;
    ros::Publisher event_pub_;
    ros::Subscriber ready_sub_;
    ros::Subscriber sync_ack_sub_;
    ros::Timer sync_timer_;
    ros::Timer statistics_timer_;
    std::map<uint64_t, ros::Timer> response_timers_;

    int participant_count_;
    int first_participant_id_;
    double sync_period_;
    double timeout_threshold_ms_;
    int max_consecutive_timeouts_;
    std::string ready_topic_;
    std::string trigger_topic_;
    std::string ack_topic_;
    std::string statistics_topic_;
    std::string events_topic_;
    double statistics_publish_period_;
    int statistics_publish_every_n_cycles_;
    int stats_window_size_;
    double slow_ratio_threshold_;
    int recovery_good_cycles_;
    double spin_rate_hz_;

    uint64_t sequence_id_;
    bool all_ready_;
    std::map<uint32_t, ParticipantStatus> participant_status_;
    std::map<uint64_t, RoundStatus> rounds_;
};

int main(int argc, char** argv) {
    ros::init(argc, argv, "sync_coordinator");
    ros::NodeHandle nh;

    SyncCoordinator coordinator(nh);
    coordinator.spin();

    return 0;
}
