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

#include "thread.h"

namespace { // these functions are placed in anonymous namespace
            // so that they are not exported at link time

extern "C" {

void cleanup_handler(void* arg) {
  delete static_cast<Callback::Callback*>(arg);
}

void* thread_func(void* arg) {

  const Callback::Callback* callback_p = static_cast<Callback::Callback*>(arg);

  // we don't need to worry about the thread being cancelled between it starting
  // and pthread_cleanup_push() executing, because a new thread always starts
  // in deferred cancellation state and pthread_cleanup_push() is not a cancellation
  // point - this new thread must therefore run as a minimum until the call to
  // Callback::dispatch() is made
  pthread_cleanup_push(cleanup_handler, const_cast<Callback::Callback*>(callback_p));

  try {
    callback_p->dispatch();
  }
  // don't bother to deal with uncaught exceptions, except Thread::Exit.  If we have
  // reached here with an uncaught exception we are in trouble anyway, and catching
  // general exceptions could have unintended consequences (eg with NPTL a catch with
  // an elipsis (...) argument without rethrowing will terminate the whole application
  // on thread cancellation; with the C++0x thread proposals thread cancellation is
  // likely to be implemented differently, with a catchable exception of some type).
  // So it is best to do nothing here and rely on the user to stop uncaught exceptions
  // reaching this far.
  catch (Thread::Exit&) {;} // just continue to thread termination

  pthread_cleanup_pop(true);

  return 0;
}

} // extern "C"
} // anonymous namespace

namespace Thread {

std::auto_ptr<Thread> Thread::start(const Callback::Callback* cb, bool joinable) {

  // take ownership of the first argument
  std::auto_ptr<Callback::Callback> cb_a(const_cast<Callback::Callback*>(cb));
  // set joinable attribute
  int detach_state;
  if (joinable) detach_state = PTHREAD_CREATE_JOINABLE;
  else detach_state = PTHREAD_CREATE_DETACHED;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, detach_state);

  Thread* instance_p;
  try {
    instance_p = new Thread;
  }
  catch (...) {
    pthread_attr_destroy(&attr);
    throw;
  }
  std::auto_ptr<Thread> return_val(instance_p);

  pthread_t thread;
  // release ownership of the callback to ThreadCB::thread_func() and
  // start the new thread
  if (!pthread_create(&thread, &attr, thread_func, cb_a.release())) {
    instance_p->thread = thread;
  }
  else {
    return_val.reset();
    // we have released in initialising thread_args
    delete cb;
  }
  pthread_attr_destroy(&attr);
  return return_val;
}

CancelBlock::CancelBlock(bool blocking) {
  // the default value of blocking is true
  if (blocking) block();
  else unblock();
}

} // namespace Thread
