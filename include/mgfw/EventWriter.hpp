#pragma once

#include "mgfw/MessageQueue.hpp"

#include <concepts>

namespace mgfw {

/**
 * Write-only sender end of a MessageQueue
 */
template<MessageType T>
class EventWriter {
public:
  explicit EventWriter(MessageQueue<T> &queue) : queue_(queue) { }

  EventWriter(const EventWriter &)            = delete;
  EventWriter &operator=(const EventWriter &) = delete;
  EventWriter(EventWriter &&)                 = default;
  EventWriter &operator=(EventWriter &&)      = default;
  ~EventWriter()                              = default;

  void write(const T &message) { queue_.enqueue(message); }

  void write(const T &&message) { queue_.enqueue(std::move(message)); }

  void write_bulk(const std::vector<T> &messages) { queue_.enqueue_bulk(messages); }

  template<typename... Args>
  requires std::constructible_from<T, Args...>
  void emplace(Args &&...args) {
    queue_.emplace(std::forward<Args>(args)...);
  }

private:
  MessageQueue<T> &queue_;
};

}  // namespace mgfw
