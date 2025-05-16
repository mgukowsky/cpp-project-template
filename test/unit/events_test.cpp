#include "mgfw/EventReader.hpp"
#include "mgfw/EventWriter.hpp"
#include "mgfw_test/LoggerMock.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

using mgfw::EventReader;
using mgfw::EventWriter;
using mgfw::MessageQueue;
using mgfw_test::LoggerMock;
using ::testing::_;
using ::testing::HasSubstr;

struct Strukt {
  int  i;
  char c;
};

TEST(MessageQueueTest, EmplaceMessage) {
  LoggerMock           logger;
  MessageQueue<Strukt> queue(logger, 1);
  EventWriter<Strukt>  writer(queue);
  EventReader<Strukt>  reader(queue);

  const int  MAGICNUM  = 42;
  const char MAGICCHAR = 'q';
  writer.emplace(MAGICNUM, MAGICCHAR);

  Strukt s{.i = 0, .c = '\0'};
  reader.drain([&](const Strukt &msg) {
    s.i = msg.i;
    s.c = msg.c;
  });

  EXPECT_EQ(MAGICNUM, s.i);
  EXPECT_EQ(MAGICCHAR, s.c);
}

TEST(MessageQueueTest, DrainQueue) {
  LoggerMock                logger;
  MessageQueue<std::string> queue(logger, 1);
  EventWriter<std::string>  writer(queue);
  EventReader<std::string>  reader(queue);

  writer.emplace("One");
  writer.emplace("Two");
  writer.emplace("Three");

  std::vector<std::string> drained;
  reader.drain([&](const std::string &msg) { drained.emplace_back(msg); });

  EXPECT_EQ(drained.size(), 3);
  EXPECT_EQ(drained[0], "One");
  EXPECT_EQ(drained[1], "Two");
  EXPECT_EQ(drained[2], "Three");
}

TEST(MessageQueueTest, LogsWarningOnDestructionIfMessagesRemain) {
  LoggerMock logger;
  EXPECT_CALL(logger, warn(HasSubstr("unprocessed message")));
  {
    MessageQueue<std::string> queue(logger, 1);
    queue.emplace("Leaked");
  }
}

TEST(MessageQueueTest, NoWarningLoggedIfNoMessagesRemain) {
  LoggerMock logger;
  EXPECT_CALL(logger, warn(_)).Times(0);
  {
    MessageQueue<std::string> queue(logger, 1);
    queue.emplace("To be drained");
    queue.drain([]([[maybe_unused]] const auto &) { });
  }
}
