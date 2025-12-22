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
  inj.add_recipe<EventReader<T>>([](Injector &injCapture, const TypeMap::InstanceId_t id) {
    auto &hive = injCapture.get<MQHive>();
    return hive.get_reader<T>(id);
  });
  inj.add_recipe<EventWriter<T>>([](Injector &injCapture, const TypeMap::InstanceId_t id) {
    auto &hive = injCapture.get<MQHive>();
    return hive.get_writer<T>(id);
  });
}

struct Msg {
  std::string s;
};

class Sender {
public:
  explicit Sender(EventWriter<Msg> writer) : writer_(std::move(writer)) { }

  void send_it() { writer_.write_bulk({{"foo"}, {"bar"}, {"baz"}}); }

private:
  EventWriter<Msg> writer_;
};

class Receiver {
public:
  explicit Receiver(EventReader<Msg> receiver, std::vector<std::string> &v)
    : receiver_(std::move(receiver)), v_(v) { }

  void get_it() {
    receiver_.drain([this](const Msg &msg) { v_.emplace_back(msg.s); });
  }

private:
  EventReader<Msg>          receiver_;
  std::vector<std::string> &v_;
};

}  // namespace

TEST(Integration, bulkMsgSend) {
  Injector inj;
  inj.bind_impl<mgfw::SpdlogLogger, mgfw::ILogger>();
  inj.add_ctor_recipe<MQHive, mgfw::ILogger &>();

  add_mq_recipe<Msg>(inj);
  inj.add_ctor_recipe<Sender, EventWriter<Msg>>();
  inj.add_ctor_recipe<Receiver, EventReader<Msg>, std::vector<std::string> &>();

  auto &sender   = inj.get<Sender>();
  auto &receiver = inj.get<Receiver>();

  sender.send_it();
  receiver.get_it();

  const auto &v = inj.get<std::vector<std::string>>();
  EXPECT_EQ("foo", v[0]);
  EXPECT_EQ("bar", v[1]);
  EXPECT_EQ("baz", v[2]);
}
