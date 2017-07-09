/* Copyright (C) 2005 and 2006 Chris Vine

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

#include <utility>
#include <unistd.h>

#include <glib/gmain.h>

#include "notifier.h"
#include "mutex.h"

bool Notifier::initialised = false;
pthread_t Notifier::thread_id;
std::set<Notifier*> Notifier::object_set;
PipeFifo Notifier::pipe;
Thread::Mutex* Notifier::set_mutex_p;
Thread::Mutex* Notifier::write_mutex_p;


void Notifier::init(void) {

  if (!initialised) {
    // slots will execute in this thread (the one in which this function
    // is called) - save the thread id so that we can detect if this object 
    // is invoked through operator()() or emit() by the same thread and avoid
    // using the pipe
    thread_id = pthread_self();

    try {
      pipe.open(PipeFifo::block);
    }
    catch (PipeError&) {
      throw PipeError("PipeFifo::open() call in Notifier::emit() cannot open pipe\n");
    }

    // these are global objects which should be initialised at program start-up
    // and last throughout program execution, so if an allocation fails just let
    // the program terminate - there is no need to do any catches to avoid
    // memory leaks
    set_mutex_p = new Thread::Mutex;
    write_mutex_p = new Thread::Mutex;
    start_iowatch(pipe.get_read_fd(),
		  sigc::ptr_fun(&Notifier::read_pipe_slot),
		  G_IO_IN);

    initialised = true;
  }
}

Notifier::Notifier(void) {
  init();

  // we need to place the address of this object in the object set.  The object
  // set is used in Notifier::read_pipe_slot() to detect a case where between
  // Notifier::emit() being called on an object in one thread and the slot
  // executing (in the slot thread) in that object after propagation through
  // the pipe, the Notifier object ceases to exist
  Thread::Mutex::Lock lock(*set_mutex_p);
  object_set.insert(this);
}

Notifier::~Notifier(void) {

  Thread::Mutex::Lock lock(*set_mutex_p);
  object_set.erase(this);
}

sigc::connection Notifier::connect(const sigc::slot<void>& cb) {

  return sig.connect(cb);
}

void Notifier::emit(void) {

  // if the same thread is signalling as is connected to the slot, call the
  // slot directly rather than pass it through the pipe
  if (in_main_thread()) {
    sig();
  }
    
  else {
    // make sure the write is atomic between threads
    Thread::Mutex::Lock lock(*write_mutex_p);
    void* instance_p = this;
    pipe.write(reinterpret_cast<char*>(&instance_p), sizeof(void*));
  }
}

bool Notifier::read_pipe_slot(void) {
  void* instance_p;
  int remaining = sizeof(void*);
  ssize_t result;
  char* temp_p = reinterpret_cast<char*>(&instance_p);
  do {
    result = Notifier::pipe.read(temp_p, remaining);
    if (result > 0) {
      temp_p += result;
      remaining -= result;
    }
  } while (remaining             // more to come
	   && result             // not end of file
	   && result != -1);     // no error

  if (result > 0) {
    if (instance_p == 0) write_error("Null pointer passed in Notifier::read_pipe_slot()\n");
    else {
      Notifier* notifier_p = static_cast<Notifier*>(instance_p);
      // check that the Notifier object concerned still exists
      { // scope block for mutex lock
	Thread::Mutex::Lock lock(*set_mutex_p);
	if (object_set.find(notifier_p) == object_set.end()) return true;
      }
      // we do not want to hold the set mutex lock here as we have no idea what is
      // in the slot connected to this signal and we could be creating deadlocks.
      // Therefore despite the use of a std::set object to check validity of this
      // Notifier object, there is still a potential race condition here if the
      // lifetime of this object is controlled by a thread other than the one in
      // which the main program event loop runs (the object's lifetime could end
      // between the set mutex lock being released above and Notifier::sig()
      // completing below).  Notifier objects are normally constructed and destroyed
      // in the same thread as that in which the main program event loop runs, but
      // where that is not the case users should use their own synchronisation methods
      // for lifetime determination
      notifier_p->sig();
    }
  }

  else { // end of file or error on pipe
    // throwing an exception in this slot is probably too extreme as it cannot readily
    // be handled - so report the error
    write_error("IO error in Notifier::read_pipe_slot()\n");
  }
  return true; // multishot
}
