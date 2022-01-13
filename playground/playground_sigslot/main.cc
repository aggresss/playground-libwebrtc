#include "rtc_base/logging.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "system_wrappers/include/sleep.h"

class Sender {
 public:
  sigslot::signal2<std::string, int> emit_signal;

  void Emit() {
    static int nVal = 0;
    char szVal[20] = {0};
    std::snprintf(szVal, 20, "signal_%d", nVal);
    emit_signal(szVal, nVal++);
  }
};

class Receiver : public sigslot::has_slots<> {
 public:
  void OnSignal(std::string strMsg, int nVal) {
    RTC_LOG(LS_INFO) << "Receive: " << strMsg.c_str() << " ==> " << nVal;
  }
};

int main() {
  rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);
  Sender sender;
  Receiver recever;

  sender.emit_signal.connect(&recever, &Receiver::OnSignal);

  while (1) {
    sender.Emit();
    webrtc::SleepMs(1000);
  }

  return 0;
}
