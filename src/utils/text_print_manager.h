/* Copyright (C) 2007 Chris Vine

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

#ifndef TEXT_PRINTMANAGER_H
#define TEXT_PRINTMANAGER_H

#include "prog_defs.h"

#include <gtk/gtkversion.h>
// we only compile this file if GTK+ version is 2.10 or higher
#if GTK_CHECK_VERSION(2,10,0)

#include <vector>
#include <string>
#include <memory>

#include <gtk/gtkwindow.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkprintoperation.h>
#include <gtk/gtkprintsettings.h>
#include <gtk/gtkprintcontext.h>
#include <gtk/gtkpagesetup.h>
#include <pango/pango-layout.h>
#include <glib-object.h>

#include <sigc++/sigc++.h>


#include "intrusive_ptr.h"
#include "gobj_handle.h"
#include "pango_layout_iter_handle.h"
#include "notifier.h"
#include "mutex.h"

/* The TextPrintManager class prints text (say, from a GtkTextBuffer
   object or from a plain text file) using the GTK+ printer interface
   provided with GTK+ 2.10.0 onwards.  To obtain a TextPrintManager
   object, call TextPrintManager::create_manager() in the thread in
   which the main GTK+ event loop runs.  TextPrintManager::set_text(),
   which passes the text to be printed, can be called in any thread.
   The text passed with TextPrintManager::set_text() must form valid
   UTF-8 (all ASCII characters form valid UTF-8).  To print the text
   entered, call TextPrintManager::print(), which may also be done in
   any thread.  The TextPrintManager::view() and
   TextPrintManager::print_to_file() methods are also available, which
   may be called in any thread.

   The TextPrintManager::page_setup() method can be used as a callback
   to display a document page setup dialog prior to printing, and this
   method should only be called in the thread in which the main GTK+
   event loop runs.  It is a static method (a TextPrintManager object
   does not need to have been created before it is called).

   Once TextPrintManager::print(), TextPrintManager::view() or
   TextPrintManager::print_to_file() has been called, the
   TextPrintManager class owns a reference to itself and so manages
   its own lifetime - so once one of the methods has been called it
   doesn't matter if the IntrusivePtr returned by
   TextPrintManager::create_manager() goes out of scope (however, once
   TextPrintManager::print(), TextPrintManager::view() or
   TextPrintManager::print_to_file() has been called, the object will
   not be deleted until both printing has completed or failed AND the
   IntrusivePtr returned by TextPrintManager::create_manager() has
   gone out of scope or has been reset()).

   Normally, you would probably only want to use any one
   TextPrintManager object to print a single print job (it has been
   designed with that in mind).  Nevertheless, if you keep a reference
   to the object alive via an active IntrusivePtr you can use it to
   print more than one print job.  However, if that is done it is not
   possible to called TextPrintManager::print()
   TextPrintManager::view(), TextPrintManager::print_to_file() or
   TextPrintManager::set_text() until the previous print job (if any)
   has been dispatched to the GTK+ print system (or cancelled).  If
   the TextPrintManager object is not ready because it is in the
   middle of handling an earlier print job, then the call to
   TextPrintManager::print(), TextPrintManager::view(),
   TextPrintManager::print_to_file() or TextPrintManager::set_text()
   will return false; if however the new print job was successfully
   started or text successfully set, they will return true.  You do
   not need to check the return value of these methods for the first
   print job despatched by any one TextPrintManager object.

   TextPrintManager::print_to_file() will also return false if no file
   name to which to print was specified.

*/

namespace { // we put the functions in anonymous namespace in text_print_manager.cpp
            // so they are not exported at link time
namespace TextPrintManagerCB {
  extern "C" {
    void text_print_begin_print(GtkPrintOperation*, GtkPrintContext*, void*);
    void text_print_draw_page(GtkPrintOperation*, GtkPrintContext*, gint, void*);
    void text_print_done(GtkPrintOperation*, GtkPrintOperationResult, void*);
    void text_print_page_setup_done(GtkPageSetup*, void*);
    GObject* text_print_create_custom_widget(GtkPrintOperation*, void*);
    void text_print_custom_widget_apply(GtkPrintOperation*, GtkWidget*, void*);
  }
}
}   // anonymous namespace

class TextPrintManager;

class TextPrintManager: public sigc::trackable, public IntrusiveLockCounter {
  enum Mode {print_mode, view_mode, file_mode} mode;

  Thread::Mutex mutex;
  GtkWindow* parent_p;
  GobjHandle<PangoLayout> text_layout_h;
  int current_line;
  PangoLayoutIterSharedHandle current_line_iter_h;
  std::auto_ptr<std::string> text_a;
  std::string file_name;
  std::vector<int> pages;
  Notifier print_notifier;
  std::string font_family;
  int font_size;
  bool ready;

  GobjHandle<GtkWidget> font_entry_h;
  GobjHandle<GtkWidget> font_size_spin_button_h;

  static GobjHandle<GtkPrintSettings> print_settings_h;
  static GobjHandle<GtkPageSetup> page_setup_h;
  static std::string default_font_family;
  static int default_font_size;

  void paginate(GtkPrintContext*);
  void print_text(void);
  void begin_print_impl(GtkPrintOperation*, GtkPrintContext*);
  void draw_page_impl(GtkPrintContext*, int);
  GObject* create_custom_widget_impl(GtkPrintOperation*);
  static void strip(std::string&);
  // private constructor
  TextPrintManager(void) {}
  // and this class may not be copied
  TextPrintManager(const TextPrintManager&);
  TextPrintManager& operator=(const TextPrintManager&);
public:
  friend void TextPrintManagerCB::text_print_begin_print(GtkPrintOperation*, GtkPrintContext*, void*);
  friend void TextPrintManagerCB::text_print_draw_page(GtkPrintOperation*, GtkPrintContext*, gint, void*);
  friend void TextPrintManagerCB::text_print_done(GtkPrintOperation*, GtkPrintOperationResult, void*);
  friend void TextPrintManagerCB::text_print_page_setup_done(GtkPageSetup*, void*);
  friend GObject* TextPrintManagerCB::text_print_create_custom_widget(GtkPrintOperation*, void*);
  friend void TextPrintManagerCB::text_print_custom_widget_apply(GtkPrintOperation*, GtkWidget*, void*);

  // if a particular font is wanted, then this can be entered as the font_family and font_size
  // arguments to the create_manager() method..  If the default of an empty string for
  // font_family and 0 for font_size is used, then printing will use the previous font settings
  // (if any), or if not a font of "Mono" and a size of 10. The fonts passed in this method can
  // in any event be overridden from the "Print font" page in the print dialog.  If a font size is
  // passed as an argument, then the value must be 0 (default) or between 8 and 24 (actual).
  static IntrusivePtr<TextPrintManager> create_manager(GtkWindow* parent = 0,
						       const std::string& font_family = "",
						       int font_size = 0);
  static void page_setup(GtkWindow* parent = 0);

  // for efficiency reasons (the string could hold a lot of text and we do not
  // want more copies than necessary hanging around) the text is passed by
  // reference (by std::auto_ptr<>).  We pass by std::auto_ptr so that the
  // string cannot be modified after it has been passed to the TextPrintManager
  // object - we take ownership of it
  bool set_text(std::auto_ptr<std::string>&);
  bool print(void);
  bool view(void);
  bool print_to_file(const char* filename);
  ~TextPrintManager(void);
};

#endif // GTK_CHECK_VERSION
#endif // TEXT_PRINTMANAGER_H
