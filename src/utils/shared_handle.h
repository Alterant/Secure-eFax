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

#ifndef SHARED_HANDLE_H
#define SHARED_HANDLE_H

#include <cstdlib>
#include <glib/gmem.h>

// define this if you want to use GLIB atomic functions/memory barriers
// instead of a (slower) mutex to lock the reference count in the
// SharedLockHandle class - to do this at least glib version 2.4 is
// required
#define SHARED_LOCK_HANDLE_USE_GLIB_ATOMIC_OPS 1


#ifdef SHARED_LOCK_HANDLE_USE_GLIB_ATOMIC_OPS
#include <glib/gutils.h>
#  if GLIB_CHECK_VERSION(2,4,0)
#  include <glib/gtypes.h>
#  include <glib/gatomic.h>
#  else
#  undef SHARED_LOCK_HANDLE_USE_GLIB_ATOMIC_OPS
#  endif
#endif

#ifndef SHARED_LOCK_HANDLE_USE_GLIB_ATOMIC_OPS
#include "mutex.h"
#endif

/*
The SharedHandle class is similar to the SharedPtr class (it keeps a
reference count and deletes the handled object when the count reaches
0), but it does not have pointer semantics.  Accordingly, it can be
used to manage the memory of arrays and other objects allocated on the
heap.

Because it is useful with arrays, by default it deallocates memory
using C++ delete[].  However, if a SharedHandle object is passed a
function object type as a second template argument when instantiated,
it will use that function object to delete memory.  This enables it to
handle the memory of any object, such as objects to be deleted using
std::free() or Glib's g_free() and g_slist_free().  Note that the
deallocator must do nothing if the value of the pointer passed to it
is NULL, so if it does not do this by default a NULL condition must be
tested for (std::free(), std::delete[] and Glib's g_free() do this
test themselves).

To reflect the fact that it is just a handle for a pointer, it has
different instantiation semantics from a SharedPtr object.  A
SharedPtr object is instantiated using this syntax:

  SharedPtr<Obj> sh_ptr(new Obj);

A SharedHandle is instantiated using this syntax (note that the
instantiated handle is for type Obj* and not Obj):

  SharedHandle<Obj*> sh_handle(new Obj[n]);

Apart from the operatorT() type conversion operator (which returns the
underlying pointer), the only other method to obtain the underlying
pointer is the get() method.  If the object referenced is an array
allocated on the heap, to use indexing you could either do this:

  SharedHandle<char*> handle(new char[10]);
  handle.get()[0] = 'a';
  std::cout << handle.get()[0] << std::endl;

or this:

  SharedHandle<char*> handle(new char[10]);
  handle[0] = 'a';
  std::cout << handle[0] << std::endl;

There is also a ScopedHandle class below, which deletes its object as
soon as it goes out of scope.  It can be viewed as a SharedHandle
which cannot be assigned to or used as the argument to a copy
constructor and therefore which cannot have a reference count of more
than 1. It is used where, if you wanted pointer semantics, you might
use a const std::autoptr<>.

*/

/********************* here are some deleter classes *******************/

template <class T> class StandardArrayDelete {
public:
  void operator()(T obj_p) {
    delete[] obj_p;
  }
};

class CFree {
public:
  void operator()(void* obj_p) {
    std::free(obj_p);
  }
};

class GFree {
public:
  void operator()(gpointer obj_p) {
    g_free(obj_p);
  }
};

/********************* define some typedefs for Glib ******************/

template <class T, class Dealloc> class SharedHandle;
template <class T, class Dealloc> class ScopedHandle;

typedef SharedHandle<gchar*, GFree> GcharSharedHandle;
typedef ScopedHandle<gchar*, GFree> GcharScopedHandle;


/******************* now the handle class definitions *****************/

template <class T, class Dealloc = StandardArrayDelete<T> > class SharedHandle {

  Dealloc deleter;

  struct {
    unsigned int* ref_count_p;
    void* obj_p;
  } ref_items;

  void unreference(void) {
    --(*ref_items.ref_count_p);
    if (*ref_items.ref_count_p == 0) {
      delete ref_items.ref_count_p;
      deleter(static_cast<T>(ref_items.obj_p));
    }
  }

  void reference(void) {
    ++(*ref_items.ref_count_p);
  }

public:
  // constructor of first SharedHandle holding the referenced object
  explicit SharedHandle(T ptr = 0) {
    try {
      ref_items.ref_count_p = new unsigned int(1);
    }
    catch (...) {
      deleter(ptr); // if allocating the int referenced by ref_items.ref_count_p
                    // has failed then delete the object to be referenced to
                    // avoid a memory leak
      throw;
    }
    ref_items.obj_p = ptr;
  }

  // copy constructor
  // this method does not throw
  SharedHandle(const SharedHandle& sh_ptr) {
    ref_items = sh_ptr.ref_items;
    reference();
  }

  // copy assignment
  // this method does not throw provided that the destructor
  // of the handled object does not throw. If the destructor of
  // the handled object throws then the SharedHandle object may be left
  // in an invalid state - we won't make this method less efficient
  // by trying to handle something that should not happen
  SharedHandle& operator=(const SharedHandle& sh_ptr) {

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

  // these accessor methods don't throw
  T get(void) const {return static_cast<T>(ref_items.obj_p);}
  operator T(void) const {return static_cast<T>(ref_items.obj_p);}
  unsigned int get_refcount(void) const {return *ref_items.ref_count_p;}

  // destructor
  // this method does not throw provided that the destructor
  // of the handled object does not throw
  ~SharedHandle(void) {unreference();}
};

template <class T, class Dealloc = StandardArrayDelete<T> > class ScopedHandle {

  Dealloc deleter;
  void* obj_p;

  // private copy constructor and assignment operator -
  // scoped handle cannot be copied
  ScopedHandle(const ScopedHandle&);
  ScopedHandle& operator=(const ScopedHandle&);
public:
  // constructor
  // this method does not throw
  explicit ScopedHandle(T ptr): obj_p(ptr) {}

  // these accessor methods don't throw
  T get(void) const {return static_cast<T>(obj_p);}
  operator T(void) const {return static_cast<T>(obj_p);}

  // destructor
  // this method does not throw provided that the destructor
  // of the handled object does not throw
  ~ScopedHandle(void) {deleter(static_cast<T>(obj_p));}
};


/*
  Class SharedLockHandle is a version of the shared handle class which
  includes locking so that it can be accessed in multiple threads.
  Note that only the reference count is protected, so this is thread
  safe in the sense in which a raw pointer is thread safe.  A shared
  handle accessed in one thread referencing a particular object is
  thread safe as against another shared handle accessing the same
  object in a different thread.  It is thus suitable for use in
  different Std C++ containers which exist in different threads but
  which contain shared objects by reference.  But:

  1.  If the referenced object is to be modified in one thread and
      read or modified in another thread an appropriate mutex for the
      referenced object is required (unless that referenced object
      does its own locking).

  2.  If the same instance of shared handle is to be modified in one
      thread (by assigning to the handle so that it references a
      different object), and copied (assigned from or used as the
      argument of a copy constructor) or modified in another thread, a
      mutex for that instance of shared handle is required.

  3.  Objects referenced by shared handles which are objects for
      which POSIX provides no guarantees (in the main, those which are
      not built-in types), such as strings and similar containers, may
      not support concurrent reads in different threads.  That depends
      on the library implementation concerned.  If that is the case, a
      mutex for the referenced object will also be required when
      reading any given instance of such an object in more than one
      thread by dereferencing any shared handles referencing it (and
      indeed, when not using shared handles at all).
*/

template <class T, class Dealloc = StandardArrayDelete<T> > class SharedLockHandle {

  Dealloc deleter;

  struct Ref_items {
#ifndef SHARED_LOCK_HANDLE_USE_GLIB_ATOMIC_OPS
    Thread::Mutex* mutex_p;
    unsigned int* ref_count_p;
#else
    gint* ref_count_p;
#endif
    void* obj_p;
  } ref_items;

  // SharedLockHandle<T, Dealloc>::unreference() does not throw exceptions
  // because  Thread::Mutex::~Mutex(), Thread::Mutex::lock() and Thread::Mutex::unlock()
  // do not throw
  void unreference(void) {
#ifndef SHARED_LOCK_HANDLE_USE_GLIB_ATOMIC_OPS
    ref_items.mutex_p->lock();
    --(*ref_items.ref_count_p);
    if (*ref_items.ref_count_p == 0) {
      delete ref_items.ref_count_p;
      ref_items.mutex_p->unlock();
      delete ref_items.mutex_p;
      deleter(static_cast<T>(ref_items.obj_p));
    }
    else ref_items.mutex_p->unlock();
#else
    if (g_atomic_int_dec_and_test(ref_items.ref_count_p)) {
      delete ref_items.ref_count_p;
      deleter(static_cast<T>(ref_items.obj_p));
    }
#endif
  }
  
  // SharedLockHandle<T, Dealloc>::reference() does not throw exceptions because
  // Thread::Mutex::Lock::Lock() and Thread::Mutex::Lock::~Lock() do not throw
  void reference(void) {
#ifndef SHARED_LOCK_HANDLE_USE_GLIB_ATOMIC_OPS
    Thread::Mutex::Lock lock(*ref_items.mutex_p);
    ++(*ref_items.ref_count_p);
#else
    g_atomic_int_inc(ref_items.ref_count_p);
#endif
  }

public:
  // constructor of first SharedLockHandle holding the referenced object
  explicit SharedLockHandle(T ptr = 0) {
#ifndef SHARED_LOCK_HANDLE_USE_GLIB_ATOMIC_OPS
    try {
      ref_items.mutex_p = new Thread::Mutex;
    }
    catch (...) {
      deleter(ptr); // if allocating the object referenced by ref_items.mutex_p
                    // has failed then delete the object to be referenced to
                    // avoid a memory leak
      throw;
    }
    try {
      ref_items.ref_count_p = new unsigned int(1);
    }
    catch (...) {
      delete ref_items.mutex_p;
      deleter(ptr);
      throw;
    }
#else
    try {
      ref_items.ref_count_p = new gint(1);
    }
    catch (...) {
      deleter(ptr); // if allocating the int referenced by ref_items.ref_count_p
                    // has failed then delete the object to be referenced to
                    // avoid a memory leak
      throw;
    }
#endif
    ref_items.obj_p = ptr;
  }

  // copy constructor
  // this method does not throw
  SharedLockHandle(const SharedLockHandle& sh_ptr) {
    ref_items = sh_ptr.ref_items;
    reference();
  }

  // copy assignment
  // this method does not throw provided that the destructor of
  // the handled object does not throw. If the destructor of the
  // handled object throws then the SharedLockHandle object may be
  // left in an invalid state - we won't make this method less
  // efficient by trying to handle something that should not happen
  SharedLockHandle& operator=(const SharedLockHandle& sh_ptr) {

    // check whether we are already referencing this object -
    // if so make this a null op.  This will also deal with
    // self-assignment
    if (ref_items.obj_p != sh_ptr.ref_items.obj_p) {

      // first unreference any object referenced by this shared handle
      unreference();
      
      // now inherit the ref_items structure from the assigning
      // shared handle and reference the object it references
      ref_items = sh_ptr.ref_items;
      reference();
    }
    return *this;
  }

  // these accessor methods don't throw
  T get(void) const {return static_cast<T>(ref_items.obj_p);}
  operator T(void) const {return static_cast<T>(ref_items.obj_p);}
  unsigned int get_refcount(void) const {
#ifndef SHARED_LOCK_HANDLE_USE_GLIB_ATOMIC_OPS
    Thread::Mutex::Lock lock(*ref_items.mutex_p);
    return *ref_items.ref_count_p;
#else
    return g_atomic_int_get(ref_items.ref_count_p);
#endif
  }

  // destructor
  // this method does not throw provided that the destructor
  // of the handled object does not throw
  ~SharedLockHandle(void) {unreference();}
};

#endif
