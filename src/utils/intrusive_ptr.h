/* Copyright (C) 2006 Chris Vine

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

#ifndef INTRUSIVE_PTR_H
#define INTRUSIVE_PTR_H

/*
  This is a class which manages objects which maintain their own
  reference count.  It requires that the referenced object has two
  functions called ref() and unref(), which increment and decrement
  the reference count respectively.  The IntrusiveCounter or
  IntrusiveLockCounter class can be inherited from to do this, but
  they do not have to be used.  The IntrusiveLockCounter class is
  the same as the IntrusiveCounter class, except that it locks the
  reference count when it is incremented or decremented in order that
  IntrusivePtr objects in different threads can access the same
  object.  (But only the reference count is locked, not the methods of
  the referenced object.)

  The constructor which takes a raw pointer does not increment the
  reference count, so it should only normally be passed a newly
  created object which begins with a reference count of 1, assuming
  that the object will arrange to delete itself once the reference
  count falls to 0.  The IntrusiveCounter and IntrusiveLockCounter
  classes behave in this way.  (It does this to maintain consistency
  with the GobjHandle class in gobj_handle.h.)  Objects which are
  already owned by an IntrusivePtr object should therefore normally
  only be passed to another IntrusivePtr by the copy constructor (or
  assignment operator), as these increment the reference count
  automatically.  If the constructor of IntrusivePtr taking a raw
  pointer is passed an object already owned by another IntrusivePtr
  object, it will be necessary to increment the reference count by
  hand explicitly.
*/

// define this if you want to use GLIB atomic functions/memory barriers
// instead of a (slower) mutex to lock the reference count in the
// IntrusiveLockCounter class - to do this at least glib version 2.4
// is required
#define INTRUSIVE_LOCK_COUNTER_USE_GLIB_ATOMIC_OPS 1


#ifdef INTRUSIVE_LOCK_COUNTER_USE_GLIB_ATOMIC_OPS
#include <glib/gutils.h>
#  if GLIB_CHECK_VERSION(2,4,0)
#  include <glib/gtypes.h>
#  include <glib/gatomic.h>
#  else
#  undef INTRUSIVE_LOCK_COUNTER_USE_GLIB_ATOMIC_OPS
#  endif
#endif

#ifndef INTRUSIVE_LOCK_COUNTER_USE_GLIB_ATOMIC_OPS
#include "mutex.h"
#endif

template <class T> class IntrusivePtr {

  T* obj_p;

  void unreference(void) {
    if (obj_p) obj_p->unref();
  }

  void reference(void) {
    if (obj_p) obj_p->ref();
  }

public:
  // constructor of first IntrusivePtr holding the referenced object
  explicit IntrusivePtr(T* ptr = 0) {
    obj_p = ptr;
  }

  // copy constructors
  // this method does not throw
  IntrusivePtr(const IntrusivePtr& intr_ptr) {
    obj_p = intr_ptr.obj_p;
    reference();
  }

  template <class U> friend class IntrusivePtr;

  // this method does not throw
  template <class U> IntrusivePtr(const IntrusivePtr<U>& intr_ptr) {
    obj_p = intr_ptr.obj_p;
    reference();
  }

  // copy assignment
  // this method does not throw provided that the destructor
  // of the handled object does not throw.
  IntrusivePtr& operator=(const IntrusivePtr& intr_ptr) {

    // check whether we are already referencing this object -
    // if so make this a null op.  This will also deal with
    // self-assignment
    if (obj_p != intr_ptr.obj_p) {

      // first unreference any object referenced by this shared pointer
      unreference();

      // now inherit the referenced object from the assigning
      // instrusive pointer and reference the object it references
      obj_p = intr_ptr.obj_p;
      reference();
    }
    return *this;
  }

  template <class U> IntrusivePtr& operator=(const IntrusivePtr<U>& intr_ptr) {
    return operator=(IntrusivePtr(intr_ptr));
  }

  // these accessor methods don't throw
  T* get(void) const {return obj_p;}
  T& operator*(void) const {return *obj_p;}
  T* operator->(void) const {return obj_p;}

  // this method does not throw provided that the destructor
  // of the handled object does not throw.
  void reset(void) {
    unreference();
    obj_p = 0;
  }

  // destructor
  // this method does not throw provided that the destructor
  // of the handled object does not throw
  ~IntrusivePtr(void) {unreference();}
};

class IntrusiveCounter {
  unsigned int count;

  // we should not be able to copy objects of this class
  // - objects, if constructed on the heap, should be passed
  // via an IntrusivePtr object.  An object of a derived class
  // might still copy itself via its copy constructor and take
  // along a new base IntrusiveCounter object constructed via
  // the default constructor, and thus have a correct reference
  // count of 1, but derived classes should not try to provide
  // their own assignment operators.
  IntrusiveCounter(const IntrusiveCounter&);
  IntrusiveCounter& operator=(const IntrusiveCounter&);
public:
  // this method does not throw
  void ref(void) {++count;}

  // this method does not throw unless the destructor
  // of an object derived from this class throws
  void unref(void) {
    --count;
    if (count == 0) delete this;
  }

  IntrusiveCounter(void): count(1) {}
  virtual ~IntrusiveCounter(void) {}
};

class IntrusiveLockCounter {
#ifndef INTRUSIVE_LOCK_COUNTER_USE_GLIB_ATOMIC_OPS
  unsigned int count;
  Thread::Mutex mutex;
#else
  gint count;
#endif

  // we should not be able to copy objects of this class
  // - objects, if constructed on the heap, should be passed
  // via an IntrusivePtr object.  An object of a derived class
  // might still copy itself via its copy constructor and take
  // along a new base IntrusiveLockCounter object constructed via
  // the default constructor, and thus have a correct reference
  // count of 1, but derived classes should not try to provide
  // their own assignment operators.
  IntrusiveLockCounter(const IntrusiveLockCounter&);
  IntrusiveLockCounter& operator=(const IntrusiveLockCounter&);
public:
  // this method does not throw
  void ref(void) {
#ifndef INTRUSIVE_LOCK_COUNTER_USE_GLIB_ATOMIC_OPS
    Thread::Mutex::Lock lock(mutex);    
    ++count;
#else
    g_atomic_int_inc(&count);
#endif
  }

  // this method does not throw unless the destructor
  // of an object derived from this class throws
  void unref(void) {
#ifndef INTRUSIVE_LOCK_COUNTER_USE_GLIB_ATOMIC_OPS
    mutex.lock();    
    --count;
    if (count == 0) {
      mutex.unlock();
      delete this;
    }
    else mutex.unlock();
#else
    if (g_atomic_int_dec_and_test(&count)) {
      delete this;
    }
#endif
  }

  IntrusiveLockCounter(void): count(1) {}
  virtual ~IntrusiveLockCounter(void) {}
};

#endif
