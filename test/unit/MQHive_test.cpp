#include "mgfw/MQHive.hpp"

#include "mgfw_test/LoggerMock.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using mgfw::EventReader;
using mgfw::EventWriter;
using mgfw::MQHive;
using mgfw_test::LoggerMock;

struct MyEvent {
  int value;
};

struct AnotherEvent {
  std::string msg;
};

TEST(MQHiveTest, GetWriterCreatesQueueAndReturnsWriter) {
  LoggerMock logger;
  MQHive     hive(logger);

  const auto EVENT_ID = 123;
  const auto MAGICNUM = 42;

  EventWriter<MyEvent> writer = hive.get_writer<MyEvent>(EVENT_ID);
  writer.write({MAGICNUM});

  EventReader<MyEvent> reader = hive.get_reader<MyEvent>(EVENT_ID);

  int i = 0;
  reader.drain([&](const MyEvent &ev) { i = ev.value; });

  EXPECT_EQ(MAGICNUM, i);
}

TEST(MQHiveTest, ThrowsOnTypeMismatch) {
  LoggerMock logger;
  MQHive     hive(logger);

  const auto MAGICNUM = 456;
  hive.get_writer<MyEvent>(MAGICNUM);

  EXPECT_THROW({ hive.get_writer<AnotherEvent>(MAGICNUM); }, std::runtime_error);
}
