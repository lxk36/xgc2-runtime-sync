#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "swarm_sync_core/sample_envelope.hpp"

namespace swarm_sync {

struct TransportMessage {
  std::string key;
  uint64_t cycle_id{0};
  std::string participant_id;
  SampleEnvelope payload;
};

class TransportSubscription {
public:
  virtual ~TransportSubscription() = default;

  virtual const std::string& key() const = 0;
  virtual bool active() const = 0;
  virtual void unsubscribe() = 0;
};

class Transport {
public:
  using MessageCallback = std::function<void(const TransportMessage&)>;

  virtual ~Transport() = default;

  virtual bool publish(const TransportMessage& message, std::string* error = nullptr) = 0;
  virtual std::shared_ptr<TransportSubscription> subscribe(const std::string& key,
                                                           MessageCallback callback,
                                                           std::string* error = nullptr) = 0;

  virtual void unsubscribe(const std::shared_ptr<TransportSubscription>& subscription);
};

class InMemoryTransport final : public Transport {
public:
  InMemoryTransport();
  ~InMemoryTransport() override;

  bool publish(const TransportMessage& message, std::string* error = nullptr) override;
  std::shared_ptr<TransportSubscription> subscribe(const std::string& key,
                                                   MessageCallback callback,
                                                   std::string* error = nullptr) override;

private:
  class Impl;
  std::shared_ptr<Impl> impl_;
};

}  // namespace swarm_sync
