#pragma once

#include "mgfw/ILogger.hpp"
#include "mgfw/types.hpp"

#include <cassert>
#include <concepts>
#include <format>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace mgfw {

template<typename T>
concept MessageType = std::copyable<T>;

template<typename Fn_t, typename T>
concept MessageDrainCallback =
  std::invocable<Fn_t, const T &> && std::same_as<std::invoke_result_t<Fn_t, const T &>, void>;

/**
 * Simple message queue.
 *
 * *NOT* threadsafe; insertions are included to enforce that this MQ is only used on a single
 * thread.
 */
template<MessageType T>
class MessageQueue {
public:
  MessageQueue(ILogger &logger, const U64 id)
    : logger_(logger), id_(id), tid_(std::this_thread::get_id()) { }

  MessageQueue(const MessageQueue &)            = delete;
  MessageQueue &operator=(const MessageQueue &) = delete;
  MessageQueue(MessageQueue &&)                 = default;
  MessageQueue &operator=(MessageQueue &&)      = default;

  ~MessageQueue() {
    if(!messages_.empty()) {
      logger_.warn(std::format("MessageQueue {} destroyed with {} unprocessed message(s) remaining",
                               id_,
                               messages_.size()));
    }
  }

  /**
   * Enqueue a message
   */
  void enqueue(const T &message) {
    assert(std::this_thread::get_id() == tid_);
    messages_.push_back(message);
  }

  void enqueue(const T &&message) {
    assert(std::this_thread::get_id() == tid_);
    messages_.push_back(std::move(message));
  }

  /**
   * Enqueue a message by constructing it in place
   */
  template<typename... Args>
  void emplace(Args &&...args) {
    assert(std::this_thread::get_id() == tid_);
    messages_.emplace_back(std::forward<Args>(args)...);
  }

  /**
   * Invoke a callback on each element in the queue, then empty the queue
   */
  template<MessageDrainCallback<T> Callback_t>
  void drain(Callback_t &&callback) {
    assert(std::this_thread::get_id() == tid_);
    for(const auto &msg : messages_) {
      std::forward<Callback_t>(callback)(msg);
    }
    messages_.clear();
  }

private:
  std::vector<T>  messages_;
  ILogger        &logger_;
  const U64       id_;
  std::thread::id tid_;
};

}  // namespace mgfw
