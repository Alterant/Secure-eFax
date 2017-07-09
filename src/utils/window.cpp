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

#include <gtk/gtkmain.h>
#include <gtk/gtkobject.h>

#include "window.h"

namespace { // callback for internal use only

gboolean WinBaseCB::delete_event(GtkWidget*, GdkEvent*, void* data) {
  
  static_cast<WinBase*>(data)->on_delete_event();
  return true; // returning true prevents destroy sig being emitted
               // we call gtk_object_destroy() ourselves in the destructor
               // (it will also prevent the "response" signal being emitted
               // from GtkDialog objects in consequence of a delete event)
}

} // anonymous namespace


WinBase::WinBase(const char* caption, GdkPixbuf* icon_p, bool modal,
		 GtkWindow* parent_p_, GtkWindow* window_p):
			     in_exec_loop(false), is_modal(modal),
			     parent_p(parent_p_) {

  if (window_p) g_window_p = window_p;
  else {
    g_window_p = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  }
  
  if (caption) gtk_window_set_title(g_window_p, caption);

  if (is_modal) {
    gtk_window_set_modal(g_window_p, true);
    if (parent_p) {
      gtk_window_set_transient_for(g_window_p, parent_p);
      gtk_widget_set_sensitive(GTK_WIDGET(parent_p), false);
    }
  }

  g_signal_connect(G_OBJECT(g_window_p), "delete_event",
		   G_CALLBACK(WinBaseCB::delete_event), this);

  if (icon_p) gtk_window_set_icon(g_window_p, icon_p);
}

WinBase::~WinBase(void) {
  gtk_object_destroy(GTK_OBJECT(g_window_p));
  // call gtk_main_quit() if we have previously called WinBase::exec() on this
  // object and we have not reached this destructor via a call to WinBase::close()
  // (perhaps a parent window has called WinBase::close() or gtk_main_quit() via
  // one of its callbacks so hoping to destroy itself by going out of scope but
  // has caused this WinBase object to unblock instead - this further call to
  // gtk_main_quit() will balance up the recursive calls to gtk_main() to ensure
  // that both the parent and this object are unblocked and so destroyed)
  if (in_exec_loop) gtk_main_quit();
}

int WinBase::get_exec_val(void) const {
  return 0;
}

void WinBase::close(void) {
  if (is_modal && parent_p) gtk_widget_set_sensitive(GTK_WIDGET(parent_p), true);
  gtk_widget_hide_all(GTK_WIDGET(g_window_p));
  if (in_exec_loop) {
    in_exec_loop = false;
    gtk_main_quit();
  }
  // if we have not called exec(), then the dialog is self-owning
  // and it is safe and necessary to call 'delete this'
  else delete this;
}

void WinBase::on_delete_event(void) {
  close();
}

int WinBase::exec(void) {
  in_exec_loop = true;
  gtk_main();
  return get_exec_val();
}
