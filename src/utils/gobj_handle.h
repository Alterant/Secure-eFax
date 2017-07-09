/* Copyright (C) 2005 and 2007 Chris Vine

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

#ifndef GOBJ_HANDLE_H
#define GOBJ_HANDLE_H

/*
   This is a class which manages the reference count of GObjects.  It
   does not maintain its own reference count, but interfaces with that
   kept by the glib object system.

   GobjHandles are most useful to manage GObjects which are not also
   GtkObjects or GInitiallyUnowned objects - GtkObjects and
   GInitiallyUnowned objects have floating references which will
   result in them being automatically managed by the container in
   which they are held.  Nonetheless, GobjHandles can be used to hold
   GtkObjects and GInitiallyUnowned objects, as the constructor of a
   GobjHandle which takes a pointer will automatically take ownership
   of a newly created GtkObject or GInitiallyUnowned object.  In the
   case of a GtkObject with GTK+2.8 or less, a GobjHandle will do this
   by calling g_object_ref()/gtk_object_sink().  With GTK+-2.9 or
   higher, in the case of a GInitiallyUnowned object (which includes
   all GtkObjects) it calls g_object_ref_sink().  Plain GObjects do
   not need to be sunk to be owned by the GobjHandle.

   Note that g_object_ref()/gtk_object_sink() or g_object_ref_sink()
   are not called by the constructor taking a pointer if the floating
   reference has already been sunk, so if that constructor is passed
   an object already owned by a container it will be necessary to call
   g_object_ref() on it explicitly.  This behaviour will ensure that
   the handle behaves the same whether it is holding a plain GObject,
   or it is holding a GInitiallyUnowned/GtkObject object.  Generally
   however, where an object is already owned by a container, the
   object should be passed by another handle - ie by the copy
   constructor (or by the assignment operator), as those always
   increment the reference count automatically.

   In other words, invoke the constructor taking a pointer only with a
   newly created object (whether a GObject, GInitiallyUnowned or
   GtkObject object), and everything else will take care of itself. In
   this respect, GobjHandles work the same way as conventional shared
   pointer implementations managing objects allocated on free store.

   The class has operator*() and operator->() dereferencing operators,
   so has normal smart pointer functionality, but as it is intended
   for use with the normal C GObject/pango/GTK+ interfaces, ordinary
   use would involve passing the handle to a function taking a pointer
   to the contained object by means of the operatorT*() type
   conversion operator (which returns the underlying pointer), or by
   explicitly calling the get() method to obtain the underlying
   pointer.

   As of glib-2.8, g_object_ref() and g_object_unref() are thread
   safe, so with glib-2.8 or greater you can have different GobjHandle
   instances in different threads referencing the same GObject object.
   Of course, if you do so, this does not affect the need (or
   otherwise) in the particular use in question to lock anything other
   than the reference count - say when accessing the referenced object
   itself in different threads.
*/

#include <glib-object.h>
#include <gtk/gtkobject.h>
#include <gtk/gtkversion.h>

template <class T> class GobjHandle {

  T* obj_p;

  void unreference(void) {
    if (obj_p) g_object_unref(G_OBJECT(obj_p));
  }

  void reference(void) {
    if (obj_p) g_object_ref(G_OBJECT(obj_p));
  }

public:
  // constructor of first GobjHandle holding the referenced object
  // this method doesn't throw
  explicit GobjHandle(T* ptr = 0) {
    obj_p = ptr;

    // if an object with a floating reference has been passed to this constructor,
    // take ownership of it
#if GTK_CHECK_VERSION(2,9,0)
    if (obj_p && g_object_is_floating(G_OBJECT(obj_p))) {
      g_object_ref_sink(G_OBJECT(obj_p));
    }
#else
    if (obj_p && GTK_IS_OBJECT(obj_p) && GTK_OBJECT_FLOATING(GTK_OBJECT(obj_p))) {
      g_object_ref(G_OBJECT(obj_p));
      gtk_object_sink(GTK_OBJECT(obj_p));
    }
#endif
  }

  // copy constructor
  // this method doesn't throw
  GobjHandle(const GobjHandle& gobj_ptr) {
    obj_p = gobj_ptr.obj_p;
    reference();
  }

  // copy assignment
  // this method doesn't throw
  GobjHandle& operator=(const GobjHandle& gobj_ptr) {

    // check whether we are already referencing this object -
    // if so make this a null op.  This will also deal with
    // self-assignment
    if (obj_p != gobj_ptr.obj_p) {

      // first unreference any object referenced by this handle
      unreference();

      // now inherit the GObject from the assigning handle
      // and reference it
      obj_p = gobj_ptr.obj_p;
      reference();
    }
    return *this;
  }

  // these accessor methods don't throw
  T* get(void) const {return obj_p;}
  T& operator*(void) const {return *obj_p;}
  T* operator->(void) const {return obj_p;}
  operator T*(void) const {return obj_p;}

  // destructor
  // this method doesn't throw
  ~GobjHandle(void) {unreference();}
};

#endif
