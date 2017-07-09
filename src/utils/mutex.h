/* Copyright (C) 2005 to 2007 Chris Vine

The library comprised in this file or of which this file is part is
distributed by Chris Vine under the GNU Lesser General Public
License as follows:

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License, version 2.1, for more details.

   You should have received a copy of the GNU Lesser General Public
   License, version 2.1, along with this library (see the file LGPL.TXT
   which came with this source code package in the src/utils sub-directory);
   if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA, 02111-1307, USA.

*/

#ifndef MUTEX_H
#define MUTEX_H

#include <exception>
#include <pthread.h>
#include <time.h>

/*
  Define UTILS_COND_USE_MONOTONIC_CLOCK before the pre-processor
  parses this file if you don't want to use the default system clock
  for calls to Cond::timed_wait() - you will probably need to do this
  via the program configuration script by testing for the manifest
  constant CLOCK_MONOTONIC, that _POSIX_MONOTONIC_CLOCK is greater
  than 0 and that the pthread_condattr_setclock() function exists

  If UTILS_COND_USE_MONOTONIC_CLOCK is defined and a call to
  pthread_condattr_setclock() fails, the Cond object will remain
  uninitialised, and the exception CondSetClockError will be thrown by
  the constructor of the Cond object.
*/

namespace Thread {

struct CondSetClockError: public std::exception {
  virtual const char* what() const throw() {return "pthread_condattr_setclock() failed in Thread::Cond::Cond()";}
};

class Cond;

class Mutex {
  pthread_mutex_t pthr_mutex;
  
  // mutexes cannot be copied
  Mutex(const Mutex&);
  Mutex& operator=(const Mutex&);
public:
  class Lock;
  friend class Cond;

  int lock(void) {return pthread_mutex_lock(&pthr_mutex);}
  int trylock(void) {return pthread_mutex_trylock(&pthr_mutex);}
  int unlock(void) {return pthread_mutex_unlock(&pthr_mutex);}

  Mutex(void) {pthread_mutex_init(&pthr_mutex, 0);}
  ~Mutex(void) {pthread_mutex_destroy(&pthr_mutex);}
};

class Mutex::Lock {
  Mutex& mutex;

  // locks cannot be copied
  Lock(const Mutex::Lock&);
  Mutex::Lock& operator=(const Mutex::Lock&);
public:

  Lock(Mutex& mutex_): mutex(mutex_) {mutex.lock();}
  ~Lock(void) {mutex.unlock();}
};

class Cond {
  pthread_cond_t cond;

  // Cond cannot be copied
  Cond(const Cond&);
  Cond& operator=(const Cond&);
public:
  int signal(void) {return pthread_cond_signal(&cond);}
  int broadcast(void) {return pthread_cond_broadcast(&cond);}
  int wait(Mutex& mutex) {return pthread_cond_wait(&cond, &mutex.pthr_mutex);}
  int timed_wait(Mutex& mutex, const timespec& abs_time) {
    return pthread_cond_timedwait(&cond, &mutex.pthr_mutex, &abs_time);
  }

  // this is a utility function that inserts into a timespec structure the
  // current time plus a given number of milliseconds ahead, which can be
  // applied to a call to Cond::timed_wait()
  static void get_abs_time(timespec& ts, unsigned int millisec) {
#ifdef UTILS_COND_USE_MONOTONIC_CLOCK
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    unsigned long nanosec = ts.tv_nsec + ((millisec % 1000) * 1000000);
    ts.tv_sec += millisec/1000 + nanosec/1000000000;
    ts.tv_nsec = nanosec % 1000000000;
  }

  Cond(void) {
#ifdef UTILS_COND_USE_MONOTONIC_CLOCK
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    int retval = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (!retval) pthread_cond_init(&cond, &attr);
    pthread_condattr_destroy(&attr);
    if (retval) {
      throw CondSetClockError();
    }
#else
    pthread_cond_init(&cond, 0);
#endif
  }
  ~Cond(void) {pthread_cond_destroy(&cond);}
};

} // namespace Thread

#endif
