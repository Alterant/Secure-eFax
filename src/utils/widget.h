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

#ifndef WIDGET_H
#define WIDGET_H

#include <gtk/gtkwidget.h>

/* Most classes which encapsulate GTK+ objects will implement top
   level widgets derived from GtkWindow (see window.h/window.cpp for a
   WinBase class to manage the lifetime of these).  However, sometimes
   it is convenient for a class to encapsulate only part of the
   implementation of a top level widget, namely in a case where the
   main widget represented by that encapsulation is intended to be
   placed in a GTK+ container object maintained by another class
   encapsulating the top level widget.

   The MainWidgetBase class is intended to assist this, by managing
   references with g_object_ref() and gtk_object_sink() (for GTK+-2.8
   or less) or g_object_ref_sink() (for GTK+-2.9 or greater) such that
   that main widget and its children are correctly destroyed when the
   MainWidgetBase class goes out of scope or is deleted even if they
   are not subsequently placed in another GTK+ container object which
   calls g_object_ref()/gtk_object_sink() or g_object_ref_sink() on
   the main widget.

   This class therefore represents a safety feature which can be used
   simply by inheriting from it.

*/

class MainWidgetBase {
  // main widget object
  GtkWidget* g_widget_p;

  // MainWidgetBase cannot be copied
  MainWidgetBase(const MainWidgetBase&);
  MainWidgetBase& operator=(const MainWidgetBase&);
public:
  GtkWidget* get_main_widget(void) const {return g_widget_p;}
  MainWidgetBase(GtkWidget* widget_p);
  virtual ~MainWidgetBase(void);
};

#endif
