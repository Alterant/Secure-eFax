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

#include <gtk/gtkversion.h>
// we only compile this file if GTK+ version is 2.10 or higher
#if GTK_CHECK_VERSION(2,10,0)

#include <unistd.h>

#include <gtk/gtkcontainer.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkmisc.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtkenums.h>

#include <pango/pango-types.h>
#include <pango/pango-font.h>
#include <pango/pangocairo.h>
#include <cairo.h>

#include "text_print_manager.h"
#include "gerror_handle.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

// uncomment the following if the specialist write_error() function in
// prog_defs.h is not available in the application in question
/*
#include <iostream>
inline void write_error(const char* message) {
  std::cerr << message;
}
*/

// define this if asynchronous printing is permitted - this only works
// with GTK+2.10.13 or greater (printing to file doesn't work before
// GTK+2.10.14, whether synchronously or asynchronously)
#define TEXT_PRINT_MANAGER_ALLOW_ASYNC 1

#if !(GTK_CHECK_VERSION(2,10,13))
#undef TEXT_PRINT_MANAGER_ALLOW_ASYNC
#endif

const gdouble STANDARD_MARGINS = 15.0;
const double PANGO_SCALE_DBL = PANGO_SCALE;

GobjHandle<GtkPrintSettings> TextPrintManager::print_settings_h;
GobjHandle<GtkPageSetup> TextPrintManager::page_setup_h;
std::string TextPrintManager::default_font_family("Mono");
int TextPrintManager::default_font_size = 10;

namespace { // callbacks for internal use only

void TextPrintManagerCB::text_print_begin_print(GtkPrintOperation* print_operation_p,
						GtkPrintContext* context_p,
						void* data) {
  static_cast<TextPrintManager*>(data)->begin_print_impl(print_operation_p, context_p);
}

void TextPrintManagerCB::text_print_draw_page(GtkPrintOperation*,
					      GtkPrintContext* context_p,
					      gint page_nr,
					      void* data) {
  static_cast<TextPrintManager*>(data)->draw_page_impl(context_p, page_nr);
}

void TextPrintManagerCB::text_print_done(GtkPrintOperation* print_operation_p,
					 GtkPrintOperationResult result,
					 void* data) {

  TextPrintManager* instance_p = static_cast<TextPrintManager*>(data);
  if (result == GTK_PRINT_OPERATION_RESULT_ERROR) {
    write_error("Print error in TextPrintManagerCB::text_print_done()\n");
    GError* error_p;
    gtk_print_operation_get_error(print_operation_p, &error_p);
    GerrorScopedHandle handle_h(error_p);
    write_error(error_p->message);
    write_error("\n");
  }
  if (result == GTK_PRINT_OPERATION_RESULT_APPLY) {
    // save the print settings
    TextPrintManager::print_settings_h =
      GobjHandle<GtkPrintSettings>(gtk_print_operation_get_print_settings(print_operation_p));
    // and take ownership of the GtkPrintSettings object
    g_object_ref(G_OBJECT(TextPrintManager::print_settings_h.get()));
  }
  { // scope block for mutex lock
    Thread::Mutex::Lock lock(instance_p->mutex);
    instance_p->ready = true;
  }
  if (instance_p->parent_p) gtk_widget_set_sensitive(GTK_WIDGET(instance_p->parent_p), true);
  instance_p->unref();
}

void TextPrintManagerCB::text_print_page_setup_done(GtkPageSetup* page_setup_p, void* data) {

  // set some sane margins if we are not using a custom paper/margin size
  if (page_setup_p) {
    if (!gtk_paper_size_is_custom (gtk_page_setup_get_paper_size (page_setup_p))) {
      gtk_page_setup_set_top_margin(page_setup_p, STANDARD_MARGINS, GTK_UNIT_MM);
      gtk_page_setup_set_bottom_margin(page_setup_p, STANDARD_MARGINS, GTK_UNIT_MM);
      gtk_page_setup_set_left_margin(page_setup_p, STANDARD_MARGINS, GTK_UNIT_MM);
      gtk_page_setup_set_right_margin(page_setup_p, STANDARD_MARGINS, GTK_UNIT_MM);
    }
    // The documentation is unclear whether we get a new GtkPageSetup
    // object back if an existing GtkPageSetup object was passed to
    // gtk_print_run_page_setup_dialog_async().  However this is safe
    // even if we get the same object back again - see GobjHandle
    // implementation
    TextPrintManager::page_setup_h = GobjHandle<GtkPageSetup>(page_setup_p);
  }
  if (data) gtk_widget_set_sensitive(GTK_WIDGET(data), true);
}

GObject* TextPrintManagerCB::text_print_create_custom_widget(GtkPrintOperation* print_operation_p, void* data) {

  return static_cast<TextPrintManager*>(data)->create_custom_widget_impl(print_operation_p);
}

void TextPrintManagerCB::text_print_custom_widget_apply(GtkPrintOperation*, GtkWidget*, void* data) {

  TextPrintManager* instance_p = static_cast<TextPrintManager*>(data);

  instance_p->font_size =
    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(instance_p->font_size_spin_button_h.get()));
  instance_p->font_family = gtk_entry_get_text(GTK_ENTRY(instance_p->font_entry_h.get()));
  TextPrintManager::strip(instance_p->font_family);

  TextPrintManager::default_font_size = instance_p->font_size;
  TextPrintManager::default_font_family = instance_p->font_family;
}

} // anonymous namespace

TextPrintManager::~TextPrintManager(void) {

  // destroy the text string through a mutex to synchronise memory
  Thread::Mutex::Lock lock(mutex);
  text_a.reset();
  // and empty the filename string
  file_name = "";
}

IntrusivePtr<TextPrintManager> TextPrintManager::create_manager(GtkWindow* parent,
								const std::string& font_family_,
								int font_size_) {

  TextPrintManager* instance_p = new TextPrintManager;
  instance_p->print_notifier.connect(sigc::mem_fun(*instance_p, &TextPrintManager::print_text));
  instance_p->parent_p = parent;
  instance_p->font_family = font_family_;
  instance_p->font_size = font_size_;

  Thread::Mutex::Lock lock(instance_p->mutex);
  instance_p->ready = true;
  
  return IntrusivePtr<TextPrintManager>(instance_p);
}

void TextPrintManager::page_setup(GtkWindow* parent) {

  if (!print_settings_h.get()) {
    print_settings_h = GobjHandle<GtkPrintSettings>(gtk_print_settings_new());
  }

  if (parent) gtk_widget_set_sensitive(GTK_WIDGET(parent), false);
  gtk_print_run_page_setup_dialog_async(parent, page_setup_h, print_settings_h, 
					TextPrintManagerCB::text_print_page_setup_done,
					parent);
}

bool TextPrintManager::set_text(std::auto_ptr<std::string>& text_) {
  Thread::Mutex::Lock lock(mutex);
  if (!ready) return false;
  text_a = text_;
  return true;
}

bool TextPrintManager::print(void) {

  { // scope block for mutex lock
    Thread::Mutex::Lock lock(mutex);
    if (!ready) return false;

    mode = print_mode;
    // protect our innards
    ready = false;
  }

  // take ownership of ourselves
  ref();
  print_notifier();
  return true;
}

bool TextPrintManager::view(void) {
  { // scope block for mutex lock
    Thread::Mutex::Lock lock(mutex);
    if (!ready) return false;

    mode = view_mode;
    // protect our innards
    ready = false;
  }

  // take ownership of ourselves
  ref();
  print_notifier();
  return true;
}

bool TextPrintManager::print_to_file(const char* filename) {

  { // scope block for mutex lock
    Thread::Mutex::Lock lock(mutex);
    if (!ready) return false;

    file_name = filename;
    if (file_name.empty()) {
      write_error(gettext("No file to print specified"));
      return false;
    }

    mode = file_mode;
    // protect our innards
    ready = false;
  }
  // take ownership of ourselves
  ref();
  print_notifier();
  return true;
}

void TextPrintManager::print_text(void) {

  // hand back ownership to local scope so that if there is a problem
  // or an exception is thrown this method cleans itself up, and it also
  // automatically cleans up if we do not do an asynchronous print (we will
  // ref() again later in this method if we are going to do an asynchronous
  // print and everything is set up)
  IntrusivePtr<TextPrintManager> temp(this);

  if (parent_p) gtk_widget_set_sensitive(GTK_WIDGET(parent_p), false);

  GobjHandle<GtkPrintOperation> print_operation_h(gtk_print_operation_new());

  if (print_settings_h.get()) {
    gtk_print_operation_set_print_settings(print_operation_h, print_settings_h);
  }

  // set some sane margins the first time we print in this program
  if (!page_setup_h.get()) {
    // a call to gtk_page_setup_new() will create a GtkPageSetup object
    // with the default page setup including default margins
    page_setup_h = GobjHandle<GtkPageSetup>(gtk_page_setup_new());
    gtk_page_setup_set_top_margin(page_setup_h, STANDARD_MARGINS, GTK_UNIT_MM);
    gtk_page_setup_set_bottom_margin(page_setup_h, STANDARD_MARGINS, GTK_UNIT_MM);
    gtk_page_setup_set_left_margin(page_setup_h, STANDARD_MARGINS, GTK_UNIT_MM);
    gtk_page_setup_set_right_margin(page_setup_h, STANDARD_MARGINS, GTK_UNIT_MM);
  }
  gtk_print_operation_set_default_page_setup(print_operation_h, page_setup_h);
  
  g_signal_connect(G_OBJECT(print_operation_h.get()), "begin_print",
		   G_CALLBACK(TextPrintManagerCB::text_print_begin_print), this);
  g_signal_connect(G_OBJECT(print_operation_h.get()), "draw_page",
		   G_CALLBACK(TextPrintManagerCB::text_print_draw_page), this);
#ifdef TEXT_PRINT_MANAGER_ALLOW_ASYNC
  g_signal_connect(G_OBJECT(print_operation_h.get()), "done",
		   G_CALLBACK(TextPrintManagerCB::text_print_done), this);
  gtk_print_operation_set_allow_async(print_operation_h, true);
  // regain ownership of ourselves (the print system will do the final
  // unreference in the TextPrintManagerCB::text_print_done() callback)
  ref();
#endif

  GError* error_p = 0;
  GtkPrintOperationResult result;
  Mode mode_chosen;
  { // scope block for mutex lock
    Thread::Mutex::Lock lock(mutex);
    mode_chosen = mode;
  }

  if (mode_chosen == file_mode) {
    { // scope block for mutex lock
      Thread::Mutex::Lock lock(mutex);
      gtk_print_operation_set_export_filename(print_operation_h, file_name.c_str());
    }
    result = gtk_print_operation_run(print_operation_h,
				     GTK_PRINT_OPERATION_ACTION_EXPORT,
				     parent_p, &error_p);
  }
  else if (mode_chosen == view_mode) {
    result = gtk_print_operation_run(print_operation_h,
				     GTK_PRINT_OPERATION_ACTION_PREVIEW,
				     parent_p, &error_p);
  }
  else {
    g_signal_connect(G_OBJECT(print_operation_h.get()), "create_custom_widget",
		     G_CALLBACK(TextPrintManagerCB::text_print_create_custom_widget), this);
    g_signal_connect(G_OBJECT(print_operation_h.get()), "custom_widget_apply",
		     G_CALLBACK(TextPrintManagerCB::text_print_custom_widget_apply), this);
    result = gtk_print_operation_run(print_operation_h,
				     GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				     parent_p, &error_p);
  }
#ifndef TEXT_PRINT_MANAGER_ALLOW_ASYNC
  if (result == GTK_PRINT_OPERATION_RESULT_ERROR) {
    write_error("GtkPrintOperation error in TextPrintManager::print_text()\n");
    if (error_p) {
      GerrorScopedHandle handle_h(error_p);
      write_error(error_p->message);
      write_error("\n");
    }
  }
  else if (result == GTK_PRINT_OPERATION_RESULT_APPLY) {
    print_settings_h =
      GobjHandle<GtkPrintSettings>(gtk_print_operation_get_print_settings(print_operation_h.get()));
    // take ownership of the GtkPrintSettings object
    g_object_ref(G_OBJECT(print_settings_h.get()));
  }
  { // scope block for mutex lock
    Thread::Mutex::Lock lock(mutex);
    ready = true;
  }
  if (parent_p) gtk_widget_set_sensitive(GTK_WIDGET(parent_p), true);
#endif
}

void TextPrintManager::begin_print_impl(GtkPrintOperation* print_operation_p,
					GtkPrintContext* context_p) {

  // create a PangoLayout object to write text on
  text_layout_h = GobjHandle<PangoLayout>(gtk_print_context_create_pango_layout(context_p));

  // apply a font to the pango layout
  PangoFontDescription* font_description_p = pango_font_description_new();

  // we need to test for an absence of font_family and font_size settings
  // because if we printed to file we will never have set them from the dialog
  if (!font_family.empty())
    pango_font_description_set_family(font_description_p, font_family.c_str());
  else
    pango_font_description_set_family(font_description_p, default_font_family.c_str());
  if (font_size >= 8 && font_size <= 24)
    pango_font_description_set_size(font_description_p, font_size * PANGO_SCALE);
  else 
    pango_font_description_set_size(font_description_p, default_font_size * PANGO_SCALE);
  pango_font_description_set_style(font_description_p, PANGO_STYLE_NORMAL);
  pango_layout_set_font_description(text_layout_h, font_description_p);
  pango_font_description_free(font_description_p);

  // set the text width for the layout from the print context (ie the page setup settings)
  pango_layout_set_width(text_layout_h,
			 static_cast<int>(gtk_print_context_get_width(context_p) * PANGO_SCALE_DBL));
  // insert the text
  { // scope block for mutex lock
    Thread::Mutex::Lock lock(mutex);
    pango_layout_set_text(text_layout_h, text_a->data(), text_a->size());
  }

  // now paginate the inserted text - the paginate() function puts
  // the first line of each new page in the pages object.
  paginate(context_p);

  gtk_print_operation_set_n_pages(print_operation_p, pages.size());
  current_line_iter_h = PangoLayoutIterSharedHandle(pango_layout_get_iter(text_layout_h));
  current_line = 0;
}

void TextPrintManager::draw_page_impl(GtkPrintContext* context_p, int page_nr) {

  // check initial conditions
  if (page_nr >= static_cast<int>(pages.size()) || page_nr < 0) {
    write_error("Yikes, invalid page number passed to TextPrintManager::draw_page_impl()\n");
    return;
  }

  int start_line = pages[page_nr];
  int end_line;
  if (page_nr == static_cast<int>(pages.size() - 1)) { // last page
    end_line = pango_layout_get_line_count(text_layout_h); // one past the last line
  }
  else end_line = pages[page_nr + 1];
  
  // at this point, current_line and current_line_iter_h will be
  // referencing the same line
  if (start_line < current_line) {
    // we should never be in this block unless for some reason the pages are
    // being printed out of order (can that happen?)

    // reset current_line_iter_h with a fresh iterator pointing
    // to beginning of text layout, and reset current_line
    current_line_iter_h = PangoLayoutIterSharedHandle(pango_layout_get_iter(text_layout_h));
    current_line = 0;
  }

  // set the cairo surface of the print context to black and white
  // print. Is the cairo surface the same for all pages?  If so we
  // could do this in TextPrintManager::begin_print_impl()
  cairo_t* cairo_p = gtk_print_context_get_cairo_context(context_p);
  cairo_set_source_rgb(cairo_p, 0.0, 0.0, 0.0);

  double top_of_page = 0;

  // now write out the page, line by line
  for (; current_line < end_line;
       ++current_line, pango_layout_iter_next_line(current_line_iter_h)) {
    // if we are printing the whole document then this 'if' test should always
    // succeed, but it will not do so if we are printing selected pages with
    // gaps in the page sequence - in that case we may need to iterate from a
    // previous page to a later one until we are at the start of the page to
    // be printed
    if (current_line >= start_line) {
      // get the current line in layout co-ordinates
      PangoRectangle logical_rect;
      pango_layout_iter_get_line_extents(current_line_iter_h,
					 0, &logical_rect);
      // save the start line of this page in layout co-ordinates (y co-ordinate as pixels)
      if (current_line == start_line) top_of_page = logical_rect.y/PANGO_SCALE_DBL;

      // get the baseline of current line of this page in layout co-ordinates
      // (y co-ordinate as pixels)
      double baseline = pango_layout_iter_get_baseline(current_line_iter_h)/PANGO_SCALE_DBL;
      
      // get x co-ordinate of the current line (as pixels)
      double x_pos = logical_rect.x/PANGO_SCALE_DBL;

      // get the layout line
      PangoLayoutLine* line_p = pango_layout_iter_get_line(current_line_iter_h);
      // got to the same line on the cairo surface for this page by
      // beginning a cairo sub-path
      cairo_move_to(cairo_p, x_pos, baseline - top_of_page);
      // print to the cairo surface
      pango_cairo_show_layout_line(cairo_p, line_p);
    }
  }
}

void TextPrintManager::paginate(GtkPrintContext* context_p) {

  const int total_lines = pango_layout_get_line_count(text_layout_h);
  int line;
  // line_height_sum is the accumulated height of lines on any page,
  // as we have iterated through it
  double line_height_sum;
  // page_height is the (fixed) page height for the current
  // print context (derived from the page setup)
  const double page_height = gtk_print_context_get_height(context_p);
  // the beginning of the first page is always at line 0
  pages.push_back(0);
  // now put the first line of all the remaining pages in the pages object
  // we do this by adding up the line heights in the pango layout object until
  // they reach consecutive new pages
  for (line = 0, line_height_sum = 0; line < total_lines; ++line) {

    PangoRectangle logical_rect;
    pango_layout_line_get_extents(pango_layout_get_line(text_layout_h, line),
				  0, &logical_rect);
    // convert to pixels
    double line_height = logical_rect.height/PANGO_SCALE_DBL;
    // let's add 'em up, an' ride 'em out - Rolling, rolling, rolling, Rawhide!
    if (line_height_sum + line_height > page_height) {
      pages.push_back(line);
      line_height_sum = 0;
    }
    line_height_sum += line_height;
  }
}

GObject* TextPrintManager::create_custom_widget_impl(GtkPrintOperation* print_operation_p) {

  GtkWidget* alignment_p = gtk_alignment_new(0.5, 0.5, 1, 0);
  GtkTable* table_p = GTK_TABLE(gtk_table_new(2, 2, false));
  gtk_container_add(GTK_CONTAINER(alignment_p), GTK_WIDGET(table_p));
  gtk_container_set_border_width(GTK_CONTAINER(alignment_p), 12);

  GtkWidget* font_label_p = gtk_label_new(gettext("Font: "));
  GtkWidget* font_size_label_p = gtk_label_new(gettext("Font size: "));

  gtk_label_set_justify(GTK_LABEL(font_label_p), GTK_JUSTIFY_LEFT);
  gtk_label_set_justify(GTK_LABEL(font_size_label_p), GTK_JUSTIFY_LEFT);

  gtk_misc_set_alignment(GTK_MISC(font_label_p), 0, 0.5);
  gtk_misc_set_alignment(GTK_MISC(font_size_label_p), 0, 0.5);

  font_entry_h = GobjHandle<GtkWidget>(gtk_entry_new());
  if (!font_family.empty())
    gtk_entry_set_text(GTK_ENTRY(font_entry_h.get()), font_family.c_str());
  else
    gtk_entry_set_text(GTK_ENTRY(font_entry_h.get()), default_font_family.c_str());

  font_size_spin_button_h = GobjHandle<GtkWidget>(gtk_spin_button_new_with_range(8, 24, 1));
  gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(font_size_spin_button_h.get()), true);
  if (font_size)
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(font_size_spin_button_h.get()), font_size);
  else
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(font_size_spin_button_h.get()), default_font_size);
  GtkWidget* font_size_spin_button_alignment_p = gtk_alignment_new(0, 0.5, 0, 1);
  gtk_container_add(GTK_CONTAINER(font_size_spin_button_alignment_p), font_size_spin_button_h);
 
  gtk_table_attach(table_p, font_label_p,
		   0, 1, 0, 1,
		   GTK_FILL, GTK_SHRINK,
		   3, 12);
  gtk_table_attach(table_p, font_entry_h,
		   1, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   3, 12);

  gtk_table_attach(table_p, font_size_label_p,
		   0, 1, 1, 2,
		   GTK_FILL, GTK_SHRINK,
		   3, 12);
  gtk_table_attach(table_p, font_size_spin_button_alignment_p,
		   1, 2, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   3, 12);

  gtk_print_operation_set_custom_tab_label(print_operation_p, gettext("Print font"));

  gtk_widget_show_all(alignment_p);

  return G_OBJECT(alignment_p);
}

void TextPrintManager::strip(std::string& text) {

  // erase any trailing space or tab
  while (!text.empty() && text.find_last_of(" \t") == text.size() - 1) {
    text.resize(text.size() - 1);
  }
  // erase any leading space or tab
  while (!text.empty() && (text[0] == ' ' || text[0] == '\t')) {
    text.erase(0, 1);
  }
}


#endif // GTK_CHECK_VERSION
