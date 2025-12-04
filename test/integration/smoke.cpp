#include "mgfw/EventReader.hpp"
#include "mgfw/EventWriter.hpp"
#include "mgfw/Injector.hpp"
#include "mgfw/MQHive.hpp"
#include "mgfw/SpdlogLogger.hpp"
#include "mgfw/TypeMap.hpp"
#include "mgfw_test/LoggerMock.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using mgfw::EventReader;
using mgfw::EventWriter;
using mgfw::Injector;
using mgfw::MQHive;
using mgfw::TypeMap;

namespace {

template<typename T>
void add_mq_recipe(Injector &inj) {
  inj.add_recipe<EventReader<T>>([](Injector &inj, const TypeMap::InstanceId_t id) {
    auto &hive = inj.get<MQHive>();
    return hive.get_reader<T>(id);
  });
  inj.add_recipe<EventWriter<T>>([](Injector &inj, const TypeMap::InstanceId_t id) {
    auto &hive = inj.get<MQHive>();
    return hive.get_writer<T>(id);
  });
}

struct Msg {
  std::string s;
};

class Sender {
public:
  explicit Sender(EventWriter<Msg> writer) : writer_(std::move(writer)) { }

private:
  EventWriter<Msg> writer_;
};

class Receiver {
public:
  explicit Receiver(EventReader<Msg> receiver) : receiver_(std::move(receiver)) { }

private:
  EventReader<Msg> receiver_;
};

}  // namespace

TEST(Integration, bulkMsgSend) {
  Injector inj;
  inj.bind_impl<mgfw::SpdlogLogger, mgfw::ILogger>();
  inj.add_ctor_recipe<MQHive, mgfw::ILogger &>();

  add_mq_recipe<Msg>(inj);
  inj.add_ctor_recipe<Sender, EventWriter<Msg>>();
  inj.add_ctor_recipe<Receiver, EventReader<Msg>>();

  [[maybe_unused]] auto &sender   = inj.get<Sender>();
  [[maybe_unused]] auto &receiver = inj.get<Receiver>();

  ASSERT_TRUE(true);
}
