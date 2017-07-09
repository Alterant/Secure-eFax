/* Copyright (C) 2005 to 2008 Chris Vine

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

#ifndef THREAD_H
#define THREAD_H

#include <memory>

#include <pthread.h>

#include "callback.h"

namespace Thread {

class Thread {
  pthread_t thread;
  // private constructor - this class can only be created with Thread::start
  Thread(void) {}
  // and it cannot be copied - this class has single ownership semantics
  Thread(const Thread&);
  Thread& operator=(const Thread&);
public:

  // Use Thread::cancel() with great care - sometimes its use is unavoidable
  // but destructors for local objects may not be called if a thread exits
  // by virtue of a call to Thread::cancel() (that depends on the implementation)
  // - therefore for maximum portability only use it if there are plain data
  // structures/built-in types in existence in local scope when it is called
  // and if there is anything in free store to be released implement some
  // clean-ups with pthread_cleanup_push()/pthread_cleanup_pop().  This
  // should be controlled with pthread_setcancelstate() and/or the
  // CancelBlock class to choose the cancellation point
  void cancel(void) {pthread_cancel(thread);}

  void join(void) {pthread_join(thread, 0);}

  // threads have single ownership semantics - the thread will keep
  // running even if the return value of Thread::start() goes out of
  // scope (but it will no longer be possible to call any of the
  // methods in this class for it, which is fine if the thread is not
  // started as joinable and it is not intended to cancel it).
  // Thread::start() will return an empty std::autoptr<Thread> object
  // (where std::autoptr<Thread>::get() will return 0) if the thread
  // does not start correctly.  As Thread::start() creates a new
  // Thread object on freestore, it will throw if the new operator
  // throws when failing (by default it will throw std::bad_alloc on
  // failure to allocate memory), but it will throw no other
  // exception.  The first argument should be a Callback object
  // constructed on freestore with the new expression, such as by
  // using one of the convenience callback() functions in callback.h.
  // Thread::start() will take ownership of the callback object - it
  // will automatically be deleted either by the new thread when it
  // has finished with it, or by Thread::start() in this thread if the
  // attempt to start a new thread fails
  static std::auto_ptr<Thread> start(const Callback::Callback* cb, bool joinable);
};

class CancelBlock {
  // CancelBlocks cannot be copied (it is meaningless)
  CancelBlock(const CancelBlock&);
  CancelBlock& operator=(const CancelBlock&);
public:
  static int block(int& old_state) {return pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old_state);}
  static int block(void) {int old_state; return block(old_state);}
  static int unblock(int& old_state) {return pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old_state);}
  static int unblock(void) {int old_state; return unblock(old_state);}
  CancelBlock(bool blocking = true);
  ~CancelBlock(void) {unblock();}
};

// this class can be thrown (instead of calling pthread_exit()) when a thread
// wishes to terminate itself and also ensure stack unwinding, so that destructors
// of local objects are called.  It is caught automatically by the implementation
// of Thread::start() so that it will only terminate the thread throwing it and not
// the whole process.  See the Thread::cancel() method above, for use when a thread
// wishes to terminate another one, and the caveats on the use of Thread::cancel().
class Exit {};

} // namespace Thread

#endif
