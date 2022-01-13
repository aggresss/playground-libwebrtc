#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"

void func(void* arg) {
  RTC_LOG(LS_INFO) << "child thread  arg = " << (char*)arg;
}

int main() {
  rtc::PlatformThread th(func, (void*)"hello world", "child thread");
  th.Start();
  th.Stop();
  RTC_LOG(LS_INFO) << "main thread";
  return 0;
}
