/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains Macros for creating proxies for webrtc MediaStream and
// PeerConnection classes.

//
// Example usage:
//
// class TestInterface : public rtc::RefCountInterface {
//  public:
//   std::string FooA() = 0;
//   std::string FooB(bool arg1) const = 0;
//   std::string FooC(bool arg1)= 0;
//  };
//
// Note that return types can not be a const reference.
//
// class Test : public TestInterface {
// ... implementation of the interface.
// };
//
// BEGIN_PROXY_MAP(Test)
//   PROXY_METHOD0(std::string, FooA)
//   PROXY_CONSTMETHOD1(std::string, FooB, arg1)
//   PROXY_METHOD1(std::string, FooC, arg1)
// END_PROXY()
//
// The proxy can be created using TestProxy::Create(Thread*, TestInterface*).

#ifndef WEBRTC_API_PROXY_H_
#define WEBRTC_API_PROXY_H_

#include "webrtc/base/event.h"
#include "webrtc/base/thread.h"

namespace webrtc {

template <typename R>
class ReturnType {
 public:
  template<typename C, typename M>
  void Invoke(C* c, M m) { r_ = (c->*m)(); }
  template<typename C, typename M, typename T1>
  void Invoke(C* c, M m, T1 a1) { r_ = (c->*m)(a1); }
  template<typename C, typename M, typename T1, typename T2>
  void Invoke(C* c, M m, T1 a1, T2 a2) { r_ = (c->*m)(a1, a2); }
  template<typename C, typename M, typename T1, typename T2, typename T3>
  void Invoke(C* c, M m, T1 a1, T2 a2, T3 a3) { r_ = (c->*m)(a1, a2, a3); }
  template<typename C, typename M, typename T1, typename T2, typename T3,
      typename T4>
  void Invoke(C* c, M m, T1 a1, T2 a2, T3 a3, T4 a4) {
    r_ = (c->*m)(a1, a2, a3, a4);
  }
  template<typename C, typename M, typename T1, typename T2, typename T3,
     typename T4, typename T5>
  void Invoke(C* c, M m, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5) {
    r_ = (c->*m)(a1, a2, a3, a4, a5);
  }

  R value() { return r_; }

 private:
  R r_;
};

template <>
class ReturnType<void> {
 public:
  template<typename C, typename M>
  void Invoke(C* c, M m) { (c->*m)(); }
  template<typename C, typename M, typename T1>
  void Invoke(C* c, M m, T1 a1) { (c->*m)(a1); }
  template<typename C, typename M, typename T1, typename T2>
  void Invoke(C* c, M m, T1 a1, T2 a2) { (c->*m)(a1, a2); }
  template<typename C, typename M, typename T1, typename T2, typename T3>
  void Invoke(C* c, M m, T1 a1, T2 a2, T3 a3) { (c->*m)(a1, a2, a3); }

  void value() {}
};

namespace internal {

class SynchronousMethodCall
    : public rtc::MessageData,
      public rtc::MessageHandler {
 public:
  explicit SynchronousMethodCall(rtc::MessageHandler* proxy)
      : e_(), proxy_(proxy) {}
  ~SynchronousMethodCall() {}

  void Invoke(rtc::Thread* t) {
    if (t->IsCurrent()) {
      proxy_->OnMessage(NULL);
    } else {
      e_.reset(new rtc::Event(false, false));
      t->Post(this, 0);
      e_->Wait(rtc::Event::kForever);
    }
  }

 private:
  void OnMessage(rtc::Message*) { proxy_->OnMessage(NULL); e_->Set(); }
  rtc::scoped_ptr<rtc::Event> e_;
  rtc::MessageHandler* proxy_;
};

}  // namespace internal

template <typename C, typename R>
class MethodCall0 : public rtc::Message,
                    public rtc::MessageHandler {
 public:
  typedef R (C::*Method)();
  MethodCall0(C* c, Method m) : c_(c), m_(m) {}

  R Marshal(rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(t);
    return r_.value();
  }

 private:
  void OnMessage(rtc::Message*) {  r_.Invoke(c_, m_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
};

template <typename C, typename R>
class ConstMethodCall0 : public rtc::Message,
                         public rtc::MessageHandler {
 public:
  typedef R (C::*Method)() const;
  ConstMethodCall0(C* c, Method m) : c_(c), m_(m) {}

  R Marshal(rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(t);
    return r_.value();
  }

 private:
  void OnMessage(rtc::Message*) { r_.Invoke(c_, m_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
};

template <typename C, typename R,  typename T1>
class MethodCall1 : public rtc::Message,
                    public rtc::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1);
  MethodCall1(C* c, Method m, T1 a1) : c_(c), m_(m), a1_(a1) {}

  R Marshal(rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(t);
    return r_.value();
  }

 private:
  void OnMessage(rtc::Message*) { r_.Invoke(c_, m_, a1_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
};

template <typename C, typename R,  typename T1>
class ConstMethodCall1 : public rtc::Message,
                         public rtc::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1) const;
  ConstMethodCall1(C* c, Method m, T1 a1) : c_(c), m_(m), a1_(a1) {}

  R Marshal(rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(t);
    return r_.value();
  }

 private:
  void OnMessage(rtc::Message*) { r_.Invoke(c_, m_, a1_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
};

template <typename C, typename R, typename T1, typename T2>
class MethodCall2 : public rtc::Message,
                    public rtc::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1, T2 a2);
  MethodCall2(C* c, Method m, T1 a1, T2 a2) : c_(c), m_(m), a1_(a1), a2_(a2) {}

  R Marshal(rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(t);
    return r_.value();
  }

 private:
  void OnMessage(rtc::Message*) { r_.Invoke(c_, m_, a1_, a2_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
  T2 a2_;
};

template <typename C, typename R, typename T1, typename T2, typename T3>
class MethodCall3 : public rtc::Message,
                    public rtc::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1, T2 a2, T3 a3);
  MethodCall3(C* c, Method m, T1 a1, T2 a2, T3 a3)
      : c_(c), m_(m), a1_(a1), a2_(a2), a3_(a3) {}

  R Marshal(rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(t);
    return r_.value();
  }

 private:
  void OnMessage(rtc::Message*) { r_.Invoke(c_, m_, a1_, a2_, a3_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
  T2 a2_;
  T3 a3_;
};

template <typename C, typename R, typename T1, typename T2, typename T3,
    typename T4>
class MethodCall4 : public rtc::Message,
                    public rtc::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1, T2 a2, T3 a3, T4 a4);
  MethodCall4(C* c, Method m, T1 a1, T2 a2, T3 a3, T4 a4)
      : c_(c), m_(m), a1_(a1), a2_(a2), a3_(a3), a4_(a4) {}

  R Marshal(rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(t);
    return r_.value();
  }

 private:
  void OnMessage(rtc::Message*) { r_.Invoke(c_, m_, a1_, a2_, a3_, a4_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
  T2 a2_;
  T3 a3_;
  T4 a4_;
};

template <typename C, typename R, typename T1, typename T2, typename T3,
    typename T4, typename T5>
class MethodCall5 : public rtc::Message,
                    public rtc::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5);
  MethodCall5(C* c, Method m, T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
      : c_(c), m_(m), a1_(a1), a2_(a2), a3_(a3), a4_(a4), a5_(a5) {}

  R Marshal(rtc::Thread* t) {
    internal::SynchronousMethodCall(this).Invoke(t);
    return r_.value();
  }

 private:
  void OnMessage(rtc::Message*) { r_.Invoke(c_, m_, a1_, a2_, a3_, a4_, a5_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
  T2 a2_;
  T3 a3_;
  T4 a4_;
  T5 a5_;
};

// TODO(nisse): Rename this to {BEGIN|END}_SIGNALLING_PROXY_MAP, and
// the below to {BEGIN|END}_PROXY_MAP. Also rename the class to
// c##SignallingProxy.
#define BEGIN_PROXY_MAP(c)                                                \
  class c##Proxy : public c##Interface {                                  \
   protected:                                                             \
    typedef c##Interface C;                                               \
    c##Proxy(rtc::Thread* signaling_thread, C* c)                         \
      : signaling_thread_(signaling_thread), c_(c) {}                     \
    ~c##Proxy() {                                                         \
      MethodCall0<c##Proxy, void> call(this, &c##Proxy::Release_s);       \
      call.Marshal(signaling_thread_);                                    \
    }                                                                     \
                                                                          \
   public:                                                                \
    static rtc::scoped_refptr<C> Create(rtc::Thread* signaling_thread, C* c) { \
      return new rtc::RefCountedObject<c##Proxy>(signaling_thread, c);    \
    }

#define BEGIN_WORKER_PROXY_MAP(c)                                       \
  class c##Proxy : public c##Interface {                                \
   protected:                                                           \
    typedef c##Interface C;                                             \
    c##Proxy(rtc::Thread* signaling_thread, rtc::Thread* worker_thread, C* c) \
      : signaling_thread_(signaling_thread),                            \
        worker_thread_(worker_thread),                                  \
        c_(c) {}                                                        \
    ~c##Proxy() {                                                       \
      MethodCall0<c##Proxy, void> call(this, &c##Proxy::Release_s);     \
      call.Marshal(signaling_thread_);                                  \
    }                                                                   \
                                                                        \
   public:                                                              \
    static rtc::scoped_refptr<C> Create(                                \
        rtc::Thread* signaling_thread, rtc::Thread* worker_thread, C* c) { \
      return new rtc::RefCountedObject<c##Proxy>(                       \
          signaling_thread, worker_thread, c);                          \
    }

#define PROXY_METHOD0(r, method)                  \
  r method() override {                           \
    MethodCall0<C, r> call(c_.get(), &C::method); \
    return call.Marshal(signaling_thread_);       \
  }

#define PROXY_CONSTMETHOD0(r, method)                  \
  r method() const override {                          \
    ConstMethodCall0<C, r> call(c_.get(), &C::method); \
    return call.Marshal(signaling_thread_);            \
  }

#define PROXY_METHOD1(r, method, t1)                      \
  r method(t1 a1) override {                              \
    MethodCall1<C, r, t1> call(c_.get(), &C::method, a1); \
    return call.Marshal(signaling_thread_);               \
  }

#define PROXY_CONSTMETHOD1(r, method, t1)                      \
  r method(t1 a1) const override {                             \
    ConstMethodCall1<C, r, t1> call(c_.get(), &C::method, a1); \
    return call.Marshal(signaling_thread_);                    \
  }

#define PROXY_METHOD2(r, method, t1, t2)                          \
  r method(t1 a1, t2 a2) override {                               \
    MethodCall2<C, r, t1, t2> call(c_.get(), &C::method, a1, a2); \
    return call.Marshal(signaling_thread_);                       \
  }

#define PROXY_METHOD3(r, method, t1, t2, t3)                              \
  r method(t1 a1, t2 a2, t3 a3) override {                                \
    MethodCall3<C, r, t1, t2, t3> call(c_.get(), &C::method, a1, a2, a3); \
    return call.Marshal(signaling_thread_);                               \
  }

#define PROXY_METHOD4(r, method, t1, t2, t3, t4)                             \
  r method(t1 a1, t2 a2, t3 a3, t4 a4) override {                            \
    MethodCall4<C, r, t1, t2, t3, t4> call(c_.get(), &C::method, a1, a2, a3, \
                                           a4);                              \
    return call.Marshal(signaling_thread_);                                  \
  }

#define PROXY_METHOD5(r, method, t1, t2, t3, t4, t5)                         \
  r method(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) override {                     \
    MethodCall5<C, r, t1, t2, t3, t4, t5> call(c_.get(), &C::method, a1, a2, \
                                               a3, a4, a5);                  \
    return call.Marshal(signaling_thread_);                                  \
  }

// Define methods which should be invoked on the worker thread.
#define PROXY_WORKER_METHOD1(r, method, t1)               \
  r method(t1 a1) override {                              \
    MethodCall1<C, r, t1> call(c_.get(), &C::method, a1); \
    return call.Marshal(worker_thread_);                  \
  }

#define PROXY_WORKER_METHOD2(r, method, t1, t2)                   \
  r method(t1 a1, t2 a2) override {                               \
    MethodCall2<C, r, t1, t2> call(c_.get(), &C::method, a1, a2); \
    return call.Marshal(worker_thread_);                          \
  }

#define END_PROXY() \
   private:\
    void Release_s() {\
      c_ = NULL;\
    }\
    mutable rtc::Thread* signaling_thread_;\
    rtc::scoped_refptr<C> c_;\
  };\

#define END_WORKER_PROXY()                           \
   private:                                          \
    void Release_s() {                               \
      c_ = NULL;                                     \
    }                                                \
    mutable rtc::Thread* signaling_thread_;          \
    mutable rtc::Thread* worker_thread_;             \
    rtc::scoped_refptr<C> c_;                        \
  };                                                 \

}  // namespace webrtc

#endif  //  WEBRTC_API_PROXY_H_
