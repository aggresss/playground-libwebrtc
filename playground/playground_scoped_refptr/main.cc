#include "api/scoped_refptr.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_count.h"
#include "rtc_base/ref_counted_object.h"

class A : public rtc::RefCountInterface
{
 public:
  A(int i) : data_(i) { RTC_LOG(LS_INFO) << __FUNCTION__ << " : " << this; }

  void display() { RTC_LOG(LS_INFO) << "data = " << data_; }

  void set(int i) { data_ = i; }

  ~A() { RTC_LOG(LS_INFO) << __FUNCTION__; }

 private:
  int data_;
};

void func(rtc::scoped_refptr<A> sp) {
  sp->set(200);
}

int main() {
  rtc::scoped_refptr<A> sp = new rtc::RefCountedObject<A>(100);
  RTC_LOG(LS_INFO) << "sp = " << sp.get();
  sp->display();
  func(sp);
  sp->display();

  return 0;
}
