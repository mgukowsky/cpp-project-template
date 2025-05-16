#pragma once

#include "mgfw/ILogger.hpp"

#include <gmock/gmock.h>

namespace mgfw_test {

class LoggerMockKlass : public mgfw::ILogger {
public:
  MOCK_METHOD(void, critical, (std::string_view), (override));
  MOCK_METHOD(void, error, (std::string_view), (override));
  MOCK_METHOD(void, warn, (std::string_view), (override));
  MOCK_METHOD(void, info, (std::string_view), (override));
  MOCK_METHOD(void, debug, (std::string_view), (override));
  MOCK_METHOD(void, trace, (std::string_view), (override));
  MOCK_METHOD(void, set_level, (LogLevel level), (override));
};

// Consider any mock call not caught via an EXPECT_* function to be an error
using LoggerMock = ::testing::StrictMock<LoggerMockKlass>;

}  // namespace mgfw_test
