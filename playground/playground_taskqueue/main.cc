#include "system_wrappers/include/sleep.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"

#include "api/task_queue/task_queue_base.h"
#include "api/task_queue/queued_task.h"
#include "api/task_queue/default_task_queue_factory.h"

class instant_task : public webrtc::QueuedTask {
 public:
  instant_task(std::string thread_name) : thread_name_(thread_name) {}

  bool Run() override {
    RTC_LOG(LS_INFO) << thread_name_ << " post a task...";

    return true;
  }

 private:
  std::string thread_name_;
};

class delay_task : public webrtc::QueuedTask {
 public:
  delay_task(std::string thread_name) : thread_name_(thread_name) {}

  bool Run() override {
    RTC_LOG(LS_INFO) << thread_name_ << " post a task...";

    return true;
  }

 private:
  std::string thread_name_;
};

void other_thread(void* arg) {
  webrtc::TaskQueueBase* tq = (webrtc::TaskQueueBase*)arg;

  std::unique_ptr<webrtc::QueuedTask> instantTask(new instant_task("child_thread"));
  std::unique_ptr<webrtc::QueuedTask> delayedTask(new delay_task("child_thread"));

  tq->PostDelayedTask(std::move(delayedTask), 3 * 1000);

  tq->PostTask(std::move(instantTask));

  webrtc::SleepMs(5000);
}

int main() {
  std::unique_ptr<webrtc::TaskQueueFactory> tqFactory = webrtc::CreateDefaultTaskQueueFactory();

  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter> tq = tqFactory->CreateTaskQueue(
      "task queue", webrtc::TaskQueueFactory::Priority::NORMAL);

  rtc::PlatformThread other_th(other_thread, (void*)tq.get(), "child_thread");
  other_th.Start();

  webrtc::SleepMs(1000);

  std::unique_ptr<webrtc::QueuedTask> task(new instant_task("main_thread"));
  tq->PostTask(std::move(task));

  other_th.Stop();

  return 0;
}
