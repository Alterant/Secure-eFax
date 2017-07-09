/* Copyright (C) 2005 Chris Vine

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

#ifndef WINDOW_H
#define WINDOW_H

#include <gtk/gtkwindow.h>

/* This class provides a base class for lifetime management of top
   level widgets.  It would be possible to use GTK+ to control the
   lifetime of the contained GTK+ window object, through the delete
   event handler and the destroy signal.  However in the case of a
   blocking window (say a dialog) this would result in an invalid
   object remaining in scope.  It is better to use the C++ lifetime
   handling mechanisms to control object status, and this class does
   that.  Where a window is created as an auto (local) object it will
   be valid while it remains in scope.  For a window which is not
   blocking (on which exec() is not called) and which is therefore
   created on free store the WinBase class will delete its own memory
   when it is closed (see below about the close() function).

   If a NULL pointer is passed as the argument to the last parameter,
   a new window object will be created with gtk_window_new
   (GTK_WINDOW_TOPLEVEL).  However, alternatively a pre-formed object
   deriving from GtkWindow (such as a GtkDialog or GtkMessageDialog
   object) can be passed to that parameter, in which case the class
   will manage that object.

   A window will block with a call to exec(), which returns an int
   value.  The exec() method obtains its return value by calling the
   virtual protected function get_exec_val(), which by default returns
   0 but can be overridden to return something more meaningful.  If
   something other than an int needs to be returned, it will be
   necessary for a derived class to provide an extractor function to
   obtain the data from the object after exec() has returned.

   A convenience virtual protected on_delete_event() method is
   provided for any derived class which needs to respond to that
   signal.  It does not return a value (it does not cause a destroy
   event to be emitted as in the case of a GTK+ delete event handler
   returning false).  Do not call the WinBaseCB::delete_event()
   function - that is a GTK+ callback for internal use.

   If the class is constructed on free store and exec() has not been
   called on it, it self-destroys and any memory allocated to it freed
   by calling the protected close() function.  If exec() has been
   called for an object, a call to close() will only terminate the
   window event loop (that is, cause exec() to unblock), which is
   correct for an auto (local) object as it will destroy itself when
   it goes out of scope.  By default (that is, if not overridden by
   any derived classes) on_delete_event() calls close().

   No special memory management for GTK+ widgets contained in the
   WinBase object (or in an object of a class derived from WinBase) is
   needed.  The GTK+ object system takes care of that, and they will
   be released when the life of the WinBase object finishes.  For this
   reason, the WinBase class can be used to make GTK+ exception safe:
   as its constructor does not throw, exceptions thrown in the
   constructors of classes derived from it will be correctly dealt
   with without causing GTK+ to leak memory if a widget is put into
   its container in the derived class's constructor immediately after
   creation on a "top down" basis, or it is placed in another widget
   class derived from MainWidgetBase.

   If a C string is passed to the caption parameter, then the window
   will display that text as its caption.  A NULL pointer can be
   passed if no caption is required.  Likewise if a pointer to a
   GdkPixbuf object is passed as the second parameter, it will be used
   to set a window icon in the window bar (if the user's window
   manager supports this).

   The constructor of WinBase does not call gtk_widget_show() because
   the constructor of a derived class may want to do this which
   require the widget not to be visible.  Accordingly the constructor
   of the derived class (or the program user) should call
   gtk_widget_show()/gtk_widget_show_all() via WinBase::get_win()) .

*/

namespace { // we put the function in anonymous namespace in window.cpp
            // so it is not exported at link time
namespace WinBaseCB {
  extern "C" gboolean delete_event(GtkWidget*, GdkEvent*, void*);
}
}   // anonymous namespace

class WinBase {
  // main class object
  GtkWindow* g_window_p;

  bool in_exec_loop;
  bool is_modal;
  GtkWindow* parent_p;

  // WinBase cannot be copied
  WinBase(const WinBase&);
  WinBase& operator=(const WinBase&);
protected:
  void close(void);
  virtual int get_exec_val(void) const;
  virtual void on_delete_event(void);
public:
  friend gboolean WinBaseCB::delete_event(GtkWidget*, GdkEvent*, void*);

  GtkWindow* get_win(void) const {return g_window_p;}

  int exec(void);
  WinBase(const char* caption = 0, GdkPixbuf* icon_p = 0, bool modal = false,
	  GtkWindow* parent_p = 0, GtkWindow* window_p = 0);
  virtual ~WinBase(void);
};

#endif
