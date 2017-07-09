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

#include <glib-object.h>
#include <gtk/gtkobject.h>
#include <gtk/gtkversion.h>

#include "widget.h"

MainWidgetBase::MainWidgetBase(GtkWidget* widget_p): g_widget_p(widget_p) {

#if GTK_CHECK_VERSION(2,9,0)
  g_object_ref_sink(G_OBJECT(g_widget_p));
#else
  g_object_ref(G_OBJECT(g_widget_p));
  gtk_object_sink(GTK_OBJECT(g_widget_p));
#endif
}

MainWidgetBase::~MainWidgetBase(void) {
  g_object_unref(G_OBJECT(g_widget_p));
}
