/* Copyright (C) 2004 to 2006 Chris Vine

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

#ifndef SHARED_PTR_H
#define SHARED_PTR_H

// define this if you want to use GLIB atomic functions/memory barriers
// instead of a (slower) mutex to lock the reference count in the
// SharedLockPtr class - to do this at least glib version 2.4 is required
#define SHARED_LOCK_PTR_USE_GLIB_ATOMIC_OPS 1


#ifdef SHARED_LOCK_PTR_USE_GLIB_ATOMIC_OPS
#include <glib/gutils.h>
#  if GLIB_CHECK_VERSION(2,4,0)
#  include <glib/gtypes.h>
#  include <glib/gatomic.h>
#  else
#  undef SHARED_LOCK_PTR_USE_GLIB_ATOMIC_OPS
#  endif
#endif

#ifndef SHARED_LOCK_PTR_USE_GLIB_ATOMIC_OPS
#include "mutex.h"
#endif

template <class T> class SharedPtr {

  struct {
    unsigned int* ref_count_p;
    T* obj_p;
  } ref_items;

  void unreference(void) {
    --(*ref_items.ref_count_p);
    if (*ref_items.ref_count_p == 0) {
      delete ref_items.ref_count_p;
      delete ref_items.obj_p;  // if this is NULL it is still safe to apply delete to it
    }
  }

  void reference(void) {
    ++(*ref_items.ref_count_p);
  }

public:
  // constructor of first SharedPtr holding the referenced object
  explicit SharedPtr(T* ptr = 0) {
    try {
      ref_items.ref_count_p = new unsigned int(1);
    }
    catch (...) {
      delete ptr;  // if allocating the int referenced by ref_items.ref_count_p
                   // has failed then delete the object to be referenced to
                   // avoid a memory leak
      throw;
    }
    ref_items.obj_p = ptr;
  }

  // copy constructors
  // this method does not throw
  SharedPtr(const SharedPtr& sh_ptr) {
    ref_items = sh_ptr.ref_items;
    reference();
  }

  template <class U> friend class SharedPtr;

  // this method does not throw
  template <class U> SharedPtr(const SharedPtr<U>& sh_ptr) {
    // because we are allowing an implicit cast from derived to
    // base class referenced object, we need to assign from each
    // member of sh_ptr.ref_items separately
    ref_items.ref_count_p = sh_ptr.ref_items.ref_count_p;
    ref_items.obj_p = sh_ptr.ref_items.obj_p;
    reference();
  }

  // copy assignment
  // this method does not throw provided that the destructor
  // of the handled object does not throw. If the destructor of
  // the handled object throws then the SharedPtr object may be left
  // in an invalid state - we won't make this method less efficient
  // by trying to handle something that should not happen
  SharedPtr& operator=(const SharedPtr& sh_ptr) {

    // check whether we are already referencing this object -
    // if so make this a null op.  This will also deal with
    // self-assignment
    if (ref_items.obj_p != sh_ptr.ref_items.obj_p) {

      // first unreference any object referenced by this shared pointer
      unreference();

      // now inherit the ref_items structure from the assigning
      // shared pointer and reference the object it references
      ref_items = sh_ptr.ref_items;
      reference();
    }
    return *this;
  }

  template <class U> SharedPtr& operator=(const SharedPtr<U>& sh_ptr) {
    return operator=(SharedPtr(sh_ptr));
  }

  // these accessor methods don't throw
  T* get(void) const {return ref_items.obj_p;}
  T& operator*(void) const {return *ref_items.obj_p;}
  T* operator->(void) const {return ref_items.obj_p;}
  unsigned int get_refcount(void) const {return *ref_items.ref_count_p;}

  // destructor
  // this method does not throw provided that the destructor
  // of the handled object does not throw
  ~SharedPtr(void) {unreference();}
};

/*
  Class SharedLockPtr is a version of the shared pointer class which
  includes locking so that it can be accessed in multiple threads.
  Note that only the reference count is protected, so this is thread
  safe in the sense in which a raw pointer is thread safe.  A shared
  pointer accessed in one thread referencing a particular object is
  thread safe as against another shared pointer accessing the same
  object in a different thread.  It is thus suitable for use in
  different Std C++ containers which exist in different threads but
  which contain shared objects by reference.  But:

  1.  If the referenced object is to be modified in one thread and
      read or modified in another thread an appropriate mutex for the
      referenced object is required (unless that referenced object
      does its own locking).

  2.  If the same instance of shared pointer is to be modified in one
      thread (by assigning to the pointer so that it references a
      different object), and copied (assigned from or used as the
      argument of a copy constructor) or modified in another thread, a
      mutex for that instance of shared pointer is required.

  3.  Objects referenced by shared pointers which are objects for
      which POSIX provides no guarantees (in the main, those which are
      not built-in types), such as strings and similar containers, may
      not support concurrent reads in different threads.  That depends
      on the library implementation concerned.  If that is the case, a
      mutex for the referenced object will also be required when
      reading any given instance of such an object in more than one
      thread by dereferencing any shared pointers referencing it (and
      indeed, when not using shared pointers at all).
*/

template <class T> class SharedLockPtr {

  struct {
#ifndef SHARED_LOCK_PTR_USE_GLIB_ATOMIC_OPS
    Thread::Mutex* mutex_p;
    unsigned int* ref_count_p;
#else
    gint* ref_count_p;
#endif
    T* obj_p;
  } ref_items;

  // SharedLockPtr<T>::unreference() does not throw if the destructor of the
  // contained object does not throw, because  Thread::Mutex::~Mutex(),
  // Thread::Mutex::lock() and Thread::Mutex::unlock() do not throw
  void unreference(void) {
#ifndef SHARED_LOCK_PTR_USE_GLIB_ATOMIC_OPS
    ref_items.mutex_p->lock();
    --(*ref_items.ref_count_p);
    if (*ref_items.ref_count_p == 0) {
      delete ref_items.ref_count_p;
      ref_items.mutex_p->unlock();
      delete ref_items.mutex_p;
      delete ref_items.obj_p;  // if this is NULL it is still safe to apply delete to it
    }
    else ref_items.mutex_p->unlock();
#else
    if (g_atomic_int_dec_and_test(ref_items.ref_count_p)) {
      delete ref_items.ref_count_p;
      delete ref_items.obj_p;  // if this is NULL it is still safe to apply delete to it
    }
#endif
  }

  // SharedLockPtr<T>::reference() does not throw because
  // Thread::Mutex::Lock::Lock() and Thread::Mutex::Lock::~Lock() do not throw
  void reference(void) {
#ifndef SHARED_LOCK_PTR_USE_GLIB_ATOMIC_OPS
    Thread::Mutex::Lock lock(*ref_items.mutex_p);
    ++(*ref_items.ref_count_p);
#else
    g_atomic_int_inc(ref_items.ref_count_p);
#endif
  }

public:
  // constructor of first SharedLockPtr holding the referenced object
  explicit SharedLockPtr(T* ptr = 0) {
#ifndef SHARED_LOCK_PTR_USE_GLIB_ATOMIC_OPS
    try {
      ref_items.mutex_p = new Thread::Mutex;
    }
    catch (...) {
      delete ptr;  // if allocating the object referenced by ref_items.mutex_p
                   // has failed then delete the object to be referenced to
                   // avoid a memory leak
      throw;
    }
    try {
      ref_items.ref_count_p = new unsigned int(1);
    }
    catch (...) {
      delete ref_items.mutex_p;
      delete ptr;
      throw;
    }
#else
    try {
      ref_items.ref_count_p = new gint(1);
    }
    catch (...) {
      delete ptr;  // if allocating the int referenced by ref_items.ref_count_p
                   // has failed then delete the object to be referenced to
                   // avoid a memory leak
      throw;
    }
#endif
    ref_items.obj_p = ptr;
  }

  // copy constructors
  // this method does not throw
  SharedLockPtr(const SharedLockPtr& sh_ptr) {
    ref_items = sh_ptr.ref_items;
    reference();
  }

  template <class U> friend class SharedLockPtr;

  // this method does not throw
  template <class U> SharedLockPtr(const SharedLockPtr<U>& sh_ptr) {
    // because we are allowing an implicit cast from derived to
    // base class referenced object, we need to assign from each
    // member of sh_ptr.ref_items separately
#ifndef SHARED_LOCK_PTR_USE_GLIB_ATOMIC_OPS
    ref_items.mutex_p = sh_ptr.ref_items.mutex_p;
#endif
    ref_items.obj_p = sh_ptr.ref_items.obj_p;
    ref_items.ref_count_p = sh_ptr.ref_items.ref_count_p;
    reference();
  }

  // copy assignment
  // this method does not throw provided that the destructor
  // of the handled object does not throw. If the destructor of
  // the handled object throws then the SharedLockPtr object may be left
  // in an invalid state - we won't make this method less efficient
  // by trying to handle something that should not happen
  SharedLockPtr& operator=(const SharedLockPtr& sh_ptr) {

    // check whether we are already referencing this object -
    // if so make this a null op.  This will also deal with
    // self-assignment
    if (ref_items.obj_p != sh_ptr.ref_items.obj_p) {

      // first unreference any object referenced by this shared pointer
      unreference();
      
      // now inherit the ref_items structure from the assigning
      // shared pointer and reference the object it references
      ref_items = sh_ptr.ref_items;
      reference();
    }
    return *this;
  }

  template <class U> SharedLockPtr& operator=(const SharedLockPtr<U>& sh_ptr) {
    return operator=(SharedLockPtr(sh_ptr));
  }
  
  // these accessor methods don't throw
  T* get(void) const {return ref_items.obj_p;}
  T& operator*(void) const {return *ref_items.obj_p;}
  T* operator->(void) const {return ref_items.obj_p;}
  unsigned int get_refcount(void) const {
#ifndef SHARED_LOCK_PTR_USE_GLIB_ATOMIC_OPS
    Thread::Mutex::Lock lock(*ref_items.mutex_p);
    return *ref_items.ref_count_p;
#else
    return g_atomic_int_get(ref_items.ref_count_p);
#endif
  }

  // destructor
  // this method does not throw provided that the destructor
  // of the handled object does not throw
  ~SharedLockPtr(void) {unreference();}
};

#endif
