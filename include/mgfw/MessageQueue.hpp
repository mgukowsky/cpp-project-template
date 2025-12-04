#pragma once

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
#endif
#include "concurrentqueue.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "mgfw/ILogger.hpp"
#include "mgfw/types.hpp"

#include <cassert>
#include <concepts>
#include <format>
#include <type_traits>
#include <utility>
#include <vector>

namespace mgfw {

template<typename T>
concept MessageType = std::is_default_constructible_v<T> && (std::movable<T> || std::copyable<T>);

template<typename Fn_t, typename T>
concept MessageDrainCallback =
  std::invocable<Fn_t, const T &> && std::same_as<std::invoke_result_t<Fn_t, const T &>, void>;

/**
 * Simple message queue.
 */
template<MessageType T>
class MessageQueue {
public:
  MessageQueue(ILogger &logger, const U64 id) : logger_(logger), id_(id) { }

  MessageQueue(const MessageQueue &)            = delete;
  MessageQueue &operator=(const MessageQueue &) = delete;
  MessageQueue(MessageQueue &&)                 = default;
  MessageQueue &operator=(MessageQueue &&)      = default;

  ~MessageQueue() {
    if(const auto approxSize = messages_.size_approx(); approxSize > 0) {
      logger_.warn(std::format(
        "MessageQueue {} destroyed with approximately {} unprocessed message(s) remaining",
        id_,
        approxSize));
    }
  }

  /**
   * Enqueue a message
   */
  void enqueue(const T &message) { messages_.enqueue(message); }

  void enqueue(T &&message) { messages_.enqueue(std::move(message)); }

  // No need to write an rvalue version since we wouldn't be moving the vector itself; rvalue refs
  // will simply bind to the const ref argument and live until this function ends.
  void enqueue_bulk(const std::vector<T> &messages) {
    messages_.enqueue_bulk(messages.begin(), messages.size());
  }

  /**
   * Enqueue a message by constructing it in place(ish)
   */
  template<typename... Args>
  requires std::constructible_from<T, Args...>
  void emplace(Args &&...args) {
    messages_.enqueue(T{std::forward<Args>(args)...});
  }

  /**
   * Invoke a callback on each element pulled from the queue, until the queue is empty
   */
  template<MessageDrainCallback<T> Callback_t>
  void drain(const Callback_t &callback) {
    T msg;
    while(messages_.try_dequeue(msg)) {
      callback(msg);
    }
  }

private:
  moodycamel::ConcurrentQueue<T> messages_;
  ILogger                       &logger_;
  const U64                      id_;
};

}  // namespace mgfw
