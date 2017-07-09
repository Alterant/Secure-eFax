/* Copyright (C) 2004 and 2006 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#ifndef TRAY_ICON_H
#define TRAY_ICON_H

#include "prog_defs.h"

#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkmenuitem.h>
#include <gdk/gdkevents.h>

#include <sigc++/sigc++.h>

#ifdef HAVE_X11_XLIB_H
extern "C" {
#include "libegg/eggtrayicon.h"
}
#else
struct EggTrayIcon;
#endif

namespace { // we put the functions in anonymous namespace in tray_item.cpp
            // so it is not exported at link time
namespace TrayItemCB {
  extern "C" {
    void embedded_cb(GtkWidget*, void*);
    void destroy_cb(GtkWidget*, void*);
    gboolean button_press_cb(GtkWidget* , GdkEventButton*, void*);
    void menuitem_activated_cb(GtkMenuItem*, void*);
  }
}
}

class TrayItem: public sigc::trackable {

  GtkWindow* prog_window_p;
  bool embedded;
  EggTrayIcon* tray_icon_p;
  GtkWidget* event_box_p;
  GtkWidget* menu_p;
  GtkTooltips* tooltips_p;

  GtkMenuItem* list_received_faxes_item_p;
  GtkMenuItem* list_sent_faxes_item_p;
  GtkMenuItem* receive_takeover_item_p;
  GtkMenuItem* receive_answer_item_p;
  GtkMenuItem* receive_standby_item_p;
  GtkMenuItem* stop_item_p;
  GtkMenuItem* quit_item_p;

  void tray_icon_destroy(void);

  // not to be copied or assigned
  void operator=(const TrayItem&);
  TrayItem(const TrayItem&);
public:
  enum MenuItem {quit, stop, receive_standby, receive_answer, receive_takeover,
		 list_received_faxes, list_sent_faxes};

  sigc::signal0<void> left_button_pressed;
  sigc::signal1<void, int> menu_item_chosen;
  sigc::signal0<int> get_state;
  sigc::signal0<int> get_new_fax_count;

  friend void TrayItemCB::embedded_cb(GtkWidget*, void*);
  friend void TrayItemCB::destroy_cb(GtkWidget*, void*);
  friend gboolean TrayItemCB::button_press_cb(GtkWidget* , GdkEventButton*, void*);
  friend void TrayItemCB::menuitem_activated_cb(GtkMenuItem*, void*);

  void set_tooltip_slot(const char* text);
  bool is_embedded(void) const {return embedded;}

  TrayItem(GtkWindow*);
};

#endif
