/* Copyright (C) 2008 Chris Vine

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

#ifndef CALLBACK_H
#define CALLBACK_H

/* These callback classes are intended for use in cases where a
   callback object may need to be handed between threads and where
   using sigc::slot would therefore be unsafe - this is because
   sigc::slot objects representing non-static methods of a class
   derived from sigc::trackable may invoke sigc::trackable functions
   at slot construction and destruction which are not thread safe.

   The templated helper Callback::make() functions make it trivial to
   create a callback object of the correct type.  A maximum of two
   arguments to pass to the relevant class method is provided for -
   but this can be extended indefinitely.  To bind to static member
   functions (or normal non-class functions), the
   Callback::make_static() helper functions are also available.

   It is used by the Thread::Thread class to start a new thread, and
   it is also used in the Callback::post() function (further
   explanation of that function's use is set out below).
*/

#include "prog_defs.h"      // for write_error()

#include <glib/gmain.h>

// uncomment the following if the specialist write_error() function in
// prog_defs.h is not available in the application in question
/*
#include <iostream>
#include <ostream>
inline void write_error(const char* message) {
  std::cerr << message;
}
*/

namespace Callback {

/* The following Callback class is the interface class that users will
   generally see, and is suitable for passing by void*, which if using
   glib and/or gthreads and/or pthreads is almost certain to happen at
   some stage.

   Because of the class's intended usage, Callback::dispatch() (and
   Callback::operator()()) return void.  The Callback class could be
   templated to provide a return value, but that would affect the
   simplicity of the interface, and if a case were to arise where a
   result is needed, an alternative is for users to pass an argument
   by pointer (or pointer to pointer) rather than have a return value.
*/
class Callback {
public:
  // because we will usually be calling through a base class pointer,
  // it would be more natural to invoke the call back through a normal
  // member function rather than by operator()()
  virtual void dispatch() const = 0;
  // but for those who want it
  void operator()() const {dispatch();}
  virtual ~Callback() {};
};

template <class T> class Callback0: public Callback {
public:
  typedef void (T::* MemFunc)();
private:
  T* obj;
  MemFunc func;
public:
  void dispatch() const {(obj->*func)();}
  Callback0(T& obj_, MemFunc func_): obj(&obj_), func(func_) {}
};

template <class T, class Arg1> class Callback1: public Callback {
public:
  typedef void (T::* MemFunc)(Arg1);
private:
  T* obj;
  MemFunc func;
  Arg1 arg1;
public:
  void dispatch() const {(obj->*func)(arg1);}
  Callback1(T& obj_, MemFunc func_, Arg1 arg1_): obj(&obj_), func(func_), arg1(arg1_) {}
};

template <class T, class Arg1, class Arg2> class Callback2: public Callback {
public:
  typedef void (T::* MemFunc)(Arg1, Arg2);
private:
  T* obj;
  MemFunc func;
  Arg1 arg1;
  Arg2 arg2;
public:
  void dispatch() const {(obj->*func)(arg1, arg2);}
  Callback2(T& obj_, MemFunc func_, Arg1 arg1_, Arg2 arg2_): obj(&obj_), func(func_),
                                                              arg1(arg1_), arg2(arg2_) {}
};

/* const versions, for binding to const methods */

template <class T> class Callback0_const: public Callback {
public:
  typedef void (T::* MemFunc)() const;
private:
  const T* obj;
  MemFunc func;
public:
  void dispatch() const {(obj->*func)();}
  Callback0_const(const T& obj_, MemFunc func_): obj(&obj_), func(func_) {}
};

template <class T, class Arg1> class Callback1_const: public Callback {
public:
  typedef void (T::* MemFunc)(Arg1) const;
private:
  const T* obj;
  MemFunc func;
  Arg1 arg1;
public:
  void dispatch() const {(obj->*func)(arg1);}
  Callback1_const(const T& obj_, MemFunc func_, Arg1 arg1_): obj(&obj_), func(func_), arg1(arg1_) {}
};

template <class T, class Arg1, class Arg2> class Callback2_const: public Callback {
public:
  typedef void (T::* MemFunc)(Arg1, Arg2) const;
private:
  const T* obj;
  MemFunc func;
  Arg1 arg1;
  Arg2 arg2;
public:
  void dispatch() const {(obj->*func)(arg1, arg2);}
  Callback2_const(const T& obj_, MemFunc func_, Arg1 arg1_, Arg2 arg2_): obj(&obj_), func(func_),
                                                                         arg1(arg1_), arg2(arg2_) {}
};

/* for static class methods and non-class functions */

class Callback0_static: public Callback {
public:
  typedef void (*Func)();
private:
  Func func;
public:
  void dispatch() const {func();}
  Callback0_static(Func func_): func(func_) {}
};

template <class Arg1> class Callback1_static: public Callback {
public:
  typedef void (*Func)(Arg1);
private:
  Func func;
  Arg1 arg1;
public:
  void dispatch() const {func(arg1);}
  Callback1_static(Func func_, Arg1 arg1_): func(func_), arg1(arg1_) {}
};

template <class Arg1, class Arg2> class Callback2_static: public Callback {
public:
  typedef void (*Func)(Arg1, Arg2);
private:
  Func func;
  Arg1 arg1;
  Arg2 arg2;
public:
  void dispatch() const {func(arg1, arg2);}
  Callback2_static(Func func_, Arg1 arg1_, Arg2 arg2_): func(func_),
			                                arg1(arg1_), arg2(arg2_) {}
};


// Convenience functions making callback objects on freestore.  These
// can for example be passed as the first argument of the
// Thread::start() method in thread.h. They are also used by the
// Callback::post() function.

template <class T>
Callback* make(T& t,
	       typename Callback0<T>::MemFunc func) {
  return new Callback0<T>(t, func);
}

template <class T, class Arg1>
Callback* make(T& t,
	       typename Callback1<T, Arg1>::MemFunc func,
	       Arg1 arg1) {
  return new Callback1<T, Arg1>(t, func, arg1);
}

template <class T, class Arg1, class Arg2>
Callback* make(T& t,
	       typename Callback2<T, Arg1, Arg2>::MemFunc func,
	       Arg1 arg1,
	       Arg2 arg2) {
  return new Callback2<T, Arg1, Arg2>(t, func, arg1, arg2);
}

/* const versions, for binding to const methods */

template <class T>
Callback* make(const T& t,
	       typename Callback0_const<T>::MemFunc func) {
  return new Callback0_const<T>(t, func);
}

template <class T, class Arg1>
Callback* make(const T& t,
	       typename Callback1_const<T, Arg1>::MemFunc func,
	       Arg1 arg1) {
  return new Callback1_const<T, Arg1>(t, func, arg1);
}

template <class T, class Arg1, class Arg2>
Callback* make(const T& t,
	       typename Callback2_const<T, Arg1, Arg2>::MemFunc func,
	       Arg1 arg1,
	       Arg2 arg2) {
  return new Callback2_const<T, Arg1, Arg2>(t, func, arg1, arg2);
}

/* for static class methods and non-class functions */

inline Callback* make_static(Callback0_static::Func func) {
  return new Callback0_static(func);
}

template <class Arg1>
Callback* make_static(typename Callback1_static<Arg1>::Func func,
		      Arg1 arg1) {
  return new Callback1_static<Arg1>(func, arg1);
}

template <class Arg1, class Arg2>
Callback* make_static(typename Callback2_static<Arg1, Arg2>::Func func,
		      Arg1 arg1,
		      Arg2 arg2) {
  return new Callback2_static<Arg1, Arg2>(func, arg1, arg2);
}


/*
The Callback::post() function is intended as a means of passing a
callback from a worker thread to the main program loop for execution
by the main program loop.  In that respect, it provides an alternative
to the Notifier class in notifier.h/notifier.cpp.  It is passed a
pointer to a Callback::Callback object created with a call to
Callback::make.

Advantages as against Notifier:

1. If there are a lot of different events requiring callbacks to be
   dispatched in the program from worker threads to the main thread,
   this avoids having separate Notifier objects for each event.

2. It is easier to pass arguments with varying values - they can be
   passed as templated arguments to the Callback::make or
   Callback::make_static functions and no special synchronisation is
   normally required (the call to g_idle_add() invokes locking of the
   main loop which will have the effect of ensuring memory
   visibility).  With a Notifier object it may be necessary to use an
   asynchronous queue to pass variable values (or to bind a reference
   to the data, thus normally requiring separate synchronisation).

Disadvantages as against Notifier:

1. Less efficient, as a new callback object has to be created on
   freestore every time the callback is invoked.

2. Where callbacks represent non-static methods of a class, it is
   possible that when the main GUI thread executes the callback, the
   relevant object no longer exists.  Either additional steps need to
   be taken to tie object lifetime to the completion of execution of
   the callback, or the function should only be used for methods of
   objects which it is known will be in existence during the whole of
   the time that the program main loop will be running.

3. Multiple callbacks relevant to a single event cannot be invoked
   from a single call for the event - each callback has to be
   separately dispatched.
*/


extern "C" gboolean callback_wrapper(void*);

inline gboolean callback_wrapper(void* data) {
  const Callback* cb = static_cast<Callback*>(data);
  try {
    cb->dispatch();
  }
  // we can't propagate exceptions from functions with C linkage
  catch (...) {
    write_error("Exception thrown in callback_wrapper() for Callback::post() function\n");
  }
  delete cb;
  return false;
}

inline void post(const Callback* cb) {
  g_idle_add(callback_wrapper, const_cast<Callback*>(cb));
}

} // namespace Callback

#endif
