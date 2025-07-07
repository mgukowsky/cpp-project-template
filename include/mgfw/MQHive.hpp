#pragma once

#include "mgfw/EventReader.hpp"
#include "mgfw/EventWriter.hpp"
#include "mgfw/ILogger.hpp"
#include "mgfw/TypeHash.hpp"
#include "mgfw/TypeString.hpp"
#include "mgfw/types.hpp"

#include <format>
#include <memory>
#include <type_traits>
#include <unordered_map>

namespace mgfw {

/**
 * Manages MessageQueues, and gives clients a facility to retrieve the reader/writer endpoints for
 * the MessageQueue corresponding to a given ID. MessageQueues are lazily initialized as they are
 * requested.
 *
 * It is considered a bug if a reader/writer for a given ID is requested for a
 * MessageQueue with a different type than the one that already exists in the MQHive.
 */
class MQHive {
public:
  explicit MQHive(ILogger &logger) : logger_(logger) { }

  template<MessageType Raw_t, typename T = std::decay_t<Raw_t>>
  EventWriter<T> get_writer(U64 id) {
    return EventWriter<T>(get_or_create_queue<T>(id));
  }

  template<MessageType Raw_t, typename T = std::decay_t<Raw_t>>
  EventReader<T> get_reader(U64 id) {
    return EventReader<T>(get_or_create_queue<T>(id));
  }

private:
  struct MQContainerBase {
    MQContainerBase(const Hash_t typeHashArg, std::string_view typeStringArg)
      : typeHash(typeHashArg), typeString(typeStringArg) { }

    virtual ~MQContainerBase() = default;

    MQContainerBase(const MQContainerBase &)            = default;
    MQContainerBase(MQContainerBase &&)                 = default;
    MQContainerBase &operator=(const MQContainerBase &) = delete;
    MQContainerBase &operator=(MQContainerBase &&)      = delete;

    const Hash_t     typeHash;
    std::string_view typeString;
  };

  template<typename T>
  struct MQContainer : public MQContainerBase {
    MQContainer(ILogger &logger, const U64 id)
      : MQContainerBase(TypeHash<T>, TypeString<T>), mq(logger, id) { }

    MessageQueue<T> mq;
  };

  template<typename T>
  MessageQueue<T> &get_or_create_queue(U64 id) {
    auto it = queueMap_.find(id);

    if(it == queueMap_.end()) {
      auto mqContainer = std::make_unique<MQContainer<T>>(logger_, id);
      [[maybe_unused]] auto [resultIt, success] =
        queueMap_.insert(std::pair{id, std::move(mqContainer)});

      it = resultIt;
    }
    else if(it->second->typeHash != TypeHash<T>) {
      throw std::runtime_error(std::format(
        "Type mismatch on MQHive::get_or_create_queue (id = {}, storedType = {}, currentType = {})",
        id,
        it->second->typeString,
        TypeString<T>));
    }

    return static_cast<MQContainer<T> *>(it->second.get())->mq;
  }

  std::unordered_map<U64, std::unique_ptr<MQContainerBase>> queueMap_;

  ILogger &logger_;
};
}  // namespace mgfw
