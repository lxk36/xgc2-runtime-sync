#include "swarm_sync_core/transport.hpp"

#include <algorithm>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

#include "swarm_sync_core/transport_keys.hpp"

namespace swarm_sync {
namespace {

void setError(std::string* error, const std::string& value) {
  if (error) {
    *error = value;
  }
}

}  // namespace

void Transport::unsubscribe(const std::shared_ptr<TransportSubscription>& subscription) {
  if (subscription) {
    subscription->unsubscribe();
  }
}

class InMemoryTransport::Impl : public std::enable_shared_from_this<InMemoryTransport::Impl> {
public:
  using Callback = Transport::MessageCallback;

  class Subscription final : public TransportSubscription {
  public:
    Subscription(std::weak_ptr<Impl> owner, std::string key, uint64_t id)
        : owner_(std::move(owner)), key_(std::move(key)), id_(id) {}

    ~Subscription() override {
      unsubscribe();
    }

    const std::string& key() const override {
      return key_;
    }

    bool active() const override {
      const auto owner = owner_.lock();
      return owner && owner->isActive(key_, id_);
    }

    void unsubscribe() override {
      const auto owner = owner_.lock();
      if (owner) {
        owner->remove(key_, id_);
      }
    }

  private:
    std::weak_ptr<Impl> owner_;
    std::string key_;
    uint64_t id_{0};
  };

  std::shared_ptr<TransportSubscription> subscribe(const std::string& key,
                                                   Callback callback,
                                                   std::string* error) {
    std::string reason;
    if (!validateTaskSampleKey(key, &reason)) {
      setError(error, reason);
      return nullptr;
    }
    if (!callback) {
      setError(error, "callback is required");
      return nullptr;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const uint64_t id = next_id_++;
    subscribers_[key].push_back(Subscriber{id, std::move(callback)});
    return std::make_shared<Subscription>(shared_from_this(), key, id);
  }

  bool publish(const TransportMessage& message, std::string* error) {
    std::string reason;
    if (!validateTaskSampleKey(message.key, &reason)) {
      setError(error, reason);
      return false;
    }

    std::vector<Callback> callbacks;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      const auto it = subscribers_.find(message.key);
      if (it != subscribers_.end()) {
        callbacks.reserve(it->second.size());
        for (const auto& subscriber : it->second) {
          callbacks.push_back(subscriber.callback);
        }
      }
    }

    for (const auto& callback : callbacks) {
      callback(message);
    }
    return true;
  }

  bool isActive(const std::string& key, uint64_t id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = subscribers_.find(key);
    if (it == subscribers_.end()) {
      return false;
    }
    return std::any_of(it->second.begin(), it->second.end(), [id](const Subscriber& subscriber) {
      return subscriber.id == id;
    });
  }

  void remove(const std::string& key, uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = subscribers_.find(key);
    if (it == subscribers_.end()) {
      return;
    }
    auto& subscribers = it->second;
    subscribers.erase(std::remove_if(subscribers.begin(),
                                     subscribers.end(),
                                     [id](const Subscriber& subscriber) {
                                       return subscriber.id == id;
                                     }),
                      subscribers.end());
    if (subscribers.empty()) {
      subscribers_.erase(it);
    }
  }

private:
  struct Subscriber {
    uint64_t id{0};
    Callback callback;
  };

  mutable std::mutex mutex_;
  std::map<std::string, std::vector<Subscriber>> subscribers_;
  uint64_t next_id_{1};
};

InMemoryTransport::InMemoryTransport() : impl_(std::make_shared<Impl>()) {}

InMemoryTransport::~InMemoryTransport() = default;

bool InMemoryTransport::publish(const TransportMessage& message, std::string* error) {
  return impl_->publish(message, error);
}

std::shared_ptr<TransportSubscription> InMemoryTransport::subscribe(const std::string& key,
                                                                    MessageCallback callback,
                                                                    std::string* error) {
  return impl_->subscribe(key, std::move(callback), error);
}

}  // namespace swarm_sync
