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

#ifndef NOTIFIER_H
#define NOTIFIER_H

/* The Notifier class provides thread-safe signalling between two
   threads. It does this through a pipe, to which an iowatch
   (WatchSource) object is attached to connect it to the Glib program
   event loop.  It is similar in effect to, though a little different
   in implementation from, a Glib::Dispatcher object from glibmm.  A
   slot is connected to the notifier, which is called in the receiving
   thread via the program event loop when operator()() (or emit()) is
   called on the Notifier object by the signalling thread.  It is
   therefore similar in effect and syntax to a (not-thread safe)
   signal of type sigc::signal0<void>.

   If the signalling thread is the same thread as that in which the
   slot connected to it will execute (which is the thread in which the
   the main Glib program event loop executes), executing the slot via
   the pipe would risk a deadlock - if the pipe fills up, the thread
   would block on write and never be able to read from the pipe to
   empty it.  Accordingly, if the object is invoked by the same thread
   as that in which the slot will execute, this is detected and the
   slot will be invoked directly, rather than via the pipe.
   Therefore, actions so invoked may be out of order with those
   invoked by the other threads.

   The main use of Notifier objects is for a worker thread to signal
   an event to the main thread in which GTK+ is executing, which
   implies that GTK+ should also be executing in the default Glib
   program event loop (GMainContext) (as will almost always be the
   case, and is the case with efax-gtk), which is the one with which
   the program first starts.  Before a Notifier object is first used,
   it is a requirement that Notifier::init() (a static member
   function) be called in the thread in which the default main Glib
   event loop executes, and any connected slots will execute in that
   thread.  Notifier::init() only needs to be called once at program
   start-up - it doesn't need to be called separately for each
   Notifier object, and can be called before any Notifier objects have
   been constructed.  If it has not been called before the
   construction of the first Notifier object has taken place, it will
   occur automatically on that first construction.  That means that if
   the first Notifier object is not constructed in the main (event
   loop) thread of the program, then Notifier::init() must be called
   explicitly before that first object is constructed.  Before
   Notifier::init() is called (or the first Notifier object created)
   g_thread_init(0) should have been called.  It is a good idea also
   that Notifier::init() should have been called (or the first
   Notifier object constructed) before the main program thread creates
   any new threads.  Then the state of Notifier::initialised will
   automatically be visible between threads.

   The Notifier::read_pipe_slot() method checks for a case where
   between the signalling thread invoking a Notifier object and the
   main program event loop calling that method the Notifier object
   ceases to exist.  However there can still be a race condition if
   the lifetime of the Notifier object is determined outside the
   thread of execution of the main program event loop - see the
   comments in the Notifier::read_pipe_slot() method in notifier.cpp.
   Normally Notifier objects are constructed and destroyed in the main
   program thread, but where that is not the case the user will need
   to take this into account and if need be provide appropriate
   synchronisation to secure the lifetime of the Notifier object until
   after the slot has been called.

   The Notifier::connect() method connects a slot to the sigc::signal
   member of the Notifier object.  If the slot function is a member
   function of a class which derives from sigc::trackable, it will
   invoke sigc methods which are not necessarily thread safe, so any
   connections to such a slot should be done in only one thread if
   additional synchronisation is to be avoided.  (A good rule is to
   invoke Notifier::connect() only in the thread in which the main
   Glib program event loop executes, which is the same thread as the
   one in which the slot itself will execute, so that automatic
   visibility and synchronisation of relevant sigc variables is
   assured without further synchronisation.)

   For a program with two GMainContext program event loops (not a
   usual case), it would be possible for a Notifier-like object to be
   initialised in the non-default GMainContext thread, and execute in
   that thread, by passing that other GMainContext object as the last
   argument when calling start_iowatch() in Notifier::Notifier().
   However, to conserve file descriptors all Notifier objects share a
   common pipe and iowatch event watch, which implies that all
   Notifier objects would also need to execute in that other thread.
   To get around this it would be possible either to templatize
   Notifier with tag types for different GMainContexts (so that there
   would be a different static pipe/iowatch object for each
   GMainContext), or to have thread-local storage for each of the
   static objects in the Notifier class, but an easier solution for
   one-off cases would be to have a version of Notifier which does not
   use static (shared) PipeFifo and iowatch objects, at the expense of
   greater use of file descriptor resources.  None of this is relevant
   to efax-gtk.

   Such a special Notifier object could also be used to signal from a
   Unix (asynchronous) signal/interrupt handler, but in that case the
   write file descriptor of the pipe should be set non-blocking to
   prevent the very unlikely but theoretically possible case (in a
   program executing in a system under extreme load) of the pipe
   filling up before being emptied by the Notifier::read_pipe_slot()
   callback function executing in the main program and so blocking in
   the handler, thus deadlocking the program.

   To pass variable data to the slot function executed by the Notifier
   object, the AsyncQueue class can be employed.
*/


#include "prog_defs.h"      // for sigc::slot, ssize_t and write_error()

#include <set>

#include <pthread.h>
#include <sigc++/sigc++.h>

#include "pipes.h"
#include "io_watch.h"

// uncomment the following if the specialist write_error() function in
// prog_defs.h is not available in the application in question
/*
#include <iostream>
#include <ostream>
inline void write_error(const char* message) {
  std::cerr << message;
}
*/

namespace Thread {
  class Mutex;
}

class Notifier;

class Notifier {

  static bool initialised;
  static pthread_t thread_id;
  static std::set<Notifier*> object_set;
  static PipeFifo pipe;
  static Thread::Mutex* set_mutex_p;
  static Thread::Mutex* write_mutex_p;
  static bool read_pipe_slot(void);
 
  sigc::signal0<void> sig;

  // Notifier objects cannot be copied
  Notifier(const Notifier&);
  Notifier& operator=(const Notifier&);
public:
  // don't make this a static member function - it can then only be called
  // by object notation after a Notifier object has first been constructed,
  // which means Notifier::init() must have been called
  bool in_main_thread(void) {return pthread_equal(thread_id, pthread_self());}

  void emit(void);
  void operator()(void) {emit();}
  sigc::connection connect(const sigc::slot<void>&);

  // init() can throw exception PipeError
  static void init(void);

  // the constructor can throw exception PipeError (if it is the first
  // Notifier object to be constructed and Notifier::init() has not
  // previously been called)
  Notifier(void);
  ~Notifier(void);
};

#endif
