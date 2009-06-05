// -*- indent-tabs-mode: nil -*-

#ifndef __ARC_THREAD_H__
#define __ARC_THREAD_H__

#include <glibmm/thread.h>

#include <arc/User.h>

namespace Arc {
  /// This module provides convenient helpers for Glibmm interface for thread management.
  /** So far it takes care of automatic initialization
     of threading environment and creation of simple detached threads.
     Always use it instead of glibmm/thread.h and keep among first
     includes. It safe to use it multiple times and to include it both
     from source files and other include files. */

  /// Defines size of stack assigned to every new thread.
  /* It seems like MacOS has very small stack per thread by default.
     So it's safer to have bigger one defined. If this value is set
     to 0 default value wil lbe used. */
  const size_t thread_stacksize = (16 * 1024 * 1024);

  /// This macro behaves like function which makes thread of class' method.
  /** It accepts class instance and full  name of
     method - like class::method. 'method' should not be static member
     of the class. Result is true if creation of thread succeeded.
     Specified instance must be valid during whole lifetime of
     thread. So probably it is safer to destroy 'instance' in
     'method' just before exiting. */
#define CreateThreadClass(instance, method) \
  { \
    Glib::Thread *thr = NULL; \
    /* ThreadLock.lock(); */ \
    try { \
      UserSwitch usw(0,0); \
      thr = Glib::Thread::create(sigc::mem_fun((instance), &method), false); \
    } catch (std::exception& e) {}; \
    /* ThreadLock.unlock(); */ \
    (thr != NULL); \
  } \

  /// Helper function to create simple thread.
  /** It takes care of all pecularities of Glib::Thread API.
     As result it runs function 'func' with argument 'arg' in a separate
     thread.
     Returns true on success. */
  bool CreateThreadFunction(void (*func)(void*), void *arg);

  /// Helper function to create simple thread.
  /** It takes care of all pecularities of Glib::Thread API.
     As result it runs function 'func' with argument 'arg' in a separate
     thread. The created thread will be joinable.
     Returns true on success. */

  bool CreateThreadFunction(void (*func)(void*), void *arg, Glib::Thread *&thr);

  /// Simple triggered condition.
  /** Provides condition and semaphor objects in one element. */
  class SimpleCondition {
  private:
    Glib::Cond cond_;
    Glib::Mutex lock_;
    bool flag_;
  public:
    SimpleCondition(void)
      : flag_(false) {}
    ~SimpleCondition(void) {
      /* race condition ? */
      broadcast();
    }
    /** Acquire semaphor */
    void lock(void) {
      lock_.lock();
    }
    /** Release semaphor */
    void unlock(void) {
      lock_.unlock();
    }
    /** Signal about condition */
    void signal(void) {
      lock_.lock();
      flag_ = true;
      cond_.signal();
      lock_.unlock();
    }
    /** Signal about condition without using semaphor */
    void signal_nonblock(void) {
      flag_ = true;
      cond_.signal();
    }
    /** Signal about condition to all waiting threads */
    void broadcast(void) {
      lock_.lock();
      flag_ = true;
      cond_.broadcast();
      lock_.unlock();
    }
    /** Wait for condition */
    void wait(void) {
      lock_.lock();
      while (!flag_)
        cond_.wait(lock_);
      flag_ = false;
      lock_.unlock();
    }
    /** Wait for condition without using semaphor */
    void wait_nonblock(void) {
      while (!flag_)
        cond_.wait(lock_);
      flag_ = false;
    }
    /** Wait for condition no longer than t milliseconds */
    bool wait(int t) {
      lock_.lock();
      Glib::TimeVal etime;
      etime.assign_current_time();
      etime.add_milliseconds(t);
      bool res(true);
      while (!flag_) {
        res = cond_.timed_wait(lock_, etime);
        if (!res)
          break;
      }
      flag_ = false;
      lock_.unlock();
      return res;
    }
    /** Reset object to initial state */
    void reset(void) {
      lock_.lock();
      flag_ = false;
      lock_.unlock();
    }
  };

  void GlibThreadInitialize(void);

  // This class initializes glibmm thread system
  class ThreadInitializer {
  public:
    ThreadInitializer(void) {
      GlibThreadInitialize();
    }
  };

  // This is done intentionally to make sure glibmm is
  // properly initialized before every module starts
  // using threads functionality. To make it work this
  // header must be included before defining any 
  // variable/class instance using static threads-related
  // elements. The simplest way to do that is to use
  // this header instead of glibmm/thread.h
  static ThreadInitializer _local_thread_initializer;

} // namespace Arc

#endif /* __ARC_THREAD_H__ */
