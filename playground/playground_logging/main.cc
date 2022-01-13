#include "rtc_base/logging.h"
#include "rtc_base/checks.h"

int main() {
  rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);

  RTC_LOG(LS_VERBOSE) << "TEST VERBOSE";
  RTC_LOG(LS_INFO) << "TEST INFO";
  RTC_LOG(LS_WARNING) << "TEST WARNING";
  RTC_LOG(LS_ERROR) << "TEST ERROR";

  RTC_CHECK(false) << "CHECK TEST PRINT\n";

  return 0;
}
