#pragma once

#include "mgfw/MessageQueue.hpp"

namespace mgfw {

/**
 * Read-only receiver end of a MessageQueue
 */
template<MessageType T>
class EventReader {
public:
  explicit EventReader(MessageQueue<T> &queue) : queue_(queue) { }

  EventReader(const EventReader &)            = delete;
  EventReader &operator=(const EventReader &) = delete;
  EventReader(EventReader &&)                 = default;
  EventReader &operator=(EventReader &&)      = default;
  ~EventReader()                              = default;

  template<MessageDrainCallback<T> Callback>
  void drain(Callback &&callback) {
    queue_.drain(std::forward<Callback>(callback));
  }

private:
  MessageQueue<T> &queue_;
};

}  // namespace mgfw
