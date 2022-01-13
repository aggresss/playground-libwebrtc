#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/thread.h"

#include "rtc_base/platform_thread.h"

#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/queued_task.h"
#include "api/task_queue/default_task_queue_factory.h"

int main() {
  rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);

  auto current_tq = webrtc::TaskQueueBase::Current();
  {
    std::unique_ptr<rtc::Thread> thread(rtc::Thread::Create());
    thread->WrapCurrent();
    RTC_CHECK_EQ(webrtc::TaskQueueBase::Current(),
              static_cast<webrtc::TaskQueueBase*>(thread.get()));
    thread->UnwrapCurrent();
  }
  RTC_CHECK_EQ(webrtc::TaskQueueBase::Current(), current_tq);

  return 0;
}
