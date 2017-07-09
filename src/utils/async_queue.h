/* Copyright (C) 2006 and 2007 Chris Vine

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

However, it is not intended that the object code of a program whose
source code instantiates a template from this file should by reason
only of that instantiation be subject to the restrictions of use in
the GNU Lesser General Public License.  With that in mind, the words
"and instantiations of templates (of any length)" shall be treated as
inserted in the fourth paragraph of section 5 of that licence after
the words "and small inline functions (ten lines or less in length)".
This does not affect any other reason why object code may be subject
to the restrictions in that licence (nor for the avoidance of doubt
does it affect the application of section 2 of that licence to
modifications of the source code in this file).

*/

/*
  AsyncQueue is a class which provides some of the functionality of a
  std::queue object (but note that the AsyncQueue::pop(value_type&
  obj) method provides the pop()ed element - see the comments below
  for the reason), except that it has mutex locking of the data
  container so as to permit push()ing and pop()ing from different
  threads.  It is therefore useful for passing data between threads,
  perhaps in response to a signal being emitted from a Notifier
  object.  Passing the data by means of a SharedLockPtr object or an
  IntrusivePtr object referencing data derived from
  IntrusiveLockCounter would be ideal.

  AsyncQueueDispatch is a class which has a blocking pop() method,
  which allows it to be waited on by a dedicated event/message
  dispatching thread for incoming work (represented by the data pushed
  onto the queue).  In the same way, it can be used to implement
  thread pools, by having threads in the pool waiting on the queue.

  By default the queues use a std::list object as their container
  because in the kind of use mentioned above they are unlikely to hold
  many objects but they can be changed to, say, a std::deque object by
  specifying it as the second template parameter
*/

#ifndef ASYNC_QUEUE_H
#define ASYNC_QUEUE_H

#include <queue>
#include <list>
#include <exception>
#include <time.h>
#include "utils/mutex.h"

struct AsyncQueuePopError: public std::exception {
  virtual const char* what() const throw() {return "Popping from empty AsyncQueue object";}
};


template <class T, class Container = std::list<T> > class AsyncQueue {
public:
  typedef typename Container::value_type value_type;
  typedef typename Container::size_type size_type;
  typedef Container container_type;
private:
  std::queue<T, Container> q;
  mutable Thread::Mutex mutex;

  // AsyncQueue objects cannot be copied - they are mainly
  // intended to pass data between two known threads
  AsyncQueue(const AsyncQueue&);
  AsyncQueue& operator=(const AsyncQueue&);
public:
  void push(const value_type& obj) {
    Thread::Mutex::Lock lock(mutex);
    q.push(obj);
  }

  // in order to complete pop() operations atomically under a single lock,
  // we have to both copy and extract the object at the front of the queue
  // in the pop() method.  To retain strong exception safety, the object
  // into which the pop()ed data is to be placed is passed as an
  // argument by reference (this avoids a copy from a temporary object
  // after the data has been extracted from the queue).  If the queue is
  // empty when a pop is attempted, this method will throw AsyncQueuePopError.
  void pop(value_type& obj) {
    Thread::Mutex::Lock lock(mutex);
    if (q.empty()) throw AsyncQueuePopError();
    obj = q.front();
    q.pop();
  }

  // this method is for use if all that is wanted is to discard the
  // element at the front of the queue.  If the queue is empty when
  // a pop is attempted, this method will throw AsyncQueuePopError.
  void pop(void) {
    Thread::Mutex::Lock lock(mutex);
    if (q.empty()) throw AsyncQueuePopError();
    q.pop();
  }

  // the result of AsyncQueue::empty() may not be valid if another
  // thread has pushed to or popped from the queue before the value
  // returned by the method is acted on.  It is provided as a utility,
  // but may not be meaningful, depending on the intended usage.
  bool empty(void) const {
    Thread::Mutex::Lock lock(mutex);
    return q.empty();
  }

  AsyncQueue(void) {}
};

template <class T, class Container = std::list<T> > class AsyncQueueDispatch {
public:
  typedef typename Container::value_type value_type;
  typedef typename Container::size_type size_type;
  typedef Container container_type;
private:
  std::queue<T, Container> q;
  mutable Thread::Mutex mutex;
  Thread::Cond cond;

  // AsyncQueueDispatch objects cannot be copied - they are mainly
  // intended to pass data between two known threads
  AsyncQueueDispatch(const AsyncQueueDispatch&);
  AsyncQueueDispatch& operator=(const AsyncQueueDispatch&);
public:
  void push(const value_type& obj) {
    Thread::Mutex::Lock lock(mutex);
    q.push(obj);
    cond.signal();
  }

  // in order to complete pop() operations atomically under a single lock,
  // we have to both copy and extract the object at the front of the queue
  // in the pop() method.  To retain strong exception safety, the object
  // into which the pop()ed data is to be placed is passed as an
  // argument by reference (this avoids a copy from a temporary object
  // after the data has been extracted from the queue).  If the queue is
  // empty when a pop is attempted, this method will throw AsyncQueuePopError.
  void pop(value_type& obj) {
    Thread::Mutex::Lock lock(mutex);
    if (q.empty()) throw AsyncQueuePopError();
    obj = q.front();
    q.pop();
  }

  // this is the blocking pop which a dispatcher thread can wait on
  void pop_dispatch(value_type& obj) {
    Thread::Mutex::Lock lock(mutex);
    while (q.empty()) cond.wait(mutex);
    obj = q.front();
    q.pop();
  }    

  // this is a timed version of pop_dispatch.  If the timeout expires
  // (or Cond::timed_wait() fails) the method will return true and do
  // nothing; otherwise it returns false and extracts from the front
  // of the queue.
  bool pop_timed_dispatch(value_type& obj, unsigned int millisec) {
    timespec ts;
    Thread::Cond::get_abs_time(ts, millisec);
    Thread::Mutex::Lock lock(mutex);
    while (q.empty()) {
      if (cond.timed_wait(mutex, ts)) return true;
    }
    obj = q.front();
    q.pop();
    return false;
  }

  // this method is for use if all that is wanted is to discard the
  // element at the front of the queue.  If the queue is empty when
  // a pop is attempted, this method will throw AsyncQueuePopError.
  void pop(void) {
    Thread::Mutex::Lock lock(mutex);
    if (q.empty()) throw AsyncQueuePopError();
    q.pop();
  }

  // the result of AsyncQueueDispatch::empty() may not be valid if
  // another thread has pushed to or popped from the queue before the
  // value returned by the method is acted on.  It is provided as a
  // utility, but may not be meaningful, depending on the intended
  // usage.
  bool empty(void) const {
    Thread::Mutex::Lock lock(mutex);
    return q.empty();
  }

  AsyncQueueDispatch(void) {}
};

#endif
