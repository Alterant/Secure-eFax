/* Copyright (C) 2004 and 2006 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#include <string>

#include <gtk/gtkimage.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkseparatormenuitem.h>
#include <gtk/gtkeventbox.h>

#include <gdk/gdkpixbuf.h>

#include "tray_icon.h"
#include "efax_controller.h"
#include "utils/gobj_handle.h"

#ifdef HAVE_OSTREAM
#include <ios>
#include <ostream>
#endif

#ifdef HAVE_STRINGSTREAM
#include <sstream>
#else
#include <strstream>
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

namespace { // callbacks for internal use only

void TrayItemCB::embedded_cb(GtkWidget*, void* data) {
  static_cast<TrayItem*>(data)->embedded = true;

  // for debugging - there is no error, we use write_error() for reporting
  //write_error("In embedded callback\n");
}

void TrayItemCB::destroy_cb(GtkWidget*, void* data) {
  TrayItem* instance_p = static_cast<TrayItem*>(data);

  instance_p->embedded = false;
  instance_p->tray_icon_destroy();
  // now bring up the program window in case it was hidden when the system tray
  // was removed from the panel
  gtk_window_present(instance_p->prog_window_p);

  // for debugging - there is no error, we use write_error() for reporting
  //write_error("In destroy callback\n");
}

gboolean TrayItemCB::button_press_cb(GtkWidget*,
				     GdkEventButton* event_p,
				     void* data) {
  TrayItem* instance_p = static_cast<TrayItem*>(data);
  
  if (event_p->button == 1 || event_p->button == 2) instance_p->left_button_pressed();
  else if (event_p->button == 3) {

    if (instance_p->get_state() == EfaxController::inactive) {
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->stop_item_p), false);
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->receive_takeover_item_p), true);
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->receive_answer_item_p), true);
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->receive_standby_item_p), true);
    }
    else  {
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->stop_item_p), true);
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->receive_takeover_item_p), false);
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->receive_answer_item_p), false);
      gtk_widget_set_sensitive(GTK_WIDGET(instance_p->receive_standby_item_p), false);
    }
    gtk_menu_popup(GTK_MENU(instance_p->menu_p), 0, 0, 0, 0,
		   event_p->button, event_p->time);
  }
  return true; // we have completed processing the event
}

void TrayItemCB::menuitem_activated_cb(GtkMenuItem* item_p, void* data) {
  TrayItem* instance_p = static_cast<TrayItem*>(data);

  if (item_p == instance_p->list_received_faxes_item_p) {
    instance_p->menu_item_chosen(TrayItem::list_received_faxes);
  }
  else if (item_p == instance_p->list_sent_faxes_item_p) {
    instance_p->menu_item_chosen(TrayItem::list_sent_faxes);
  }
  else if (item_p == instance_p->receive_takeover_item_p) {
    instance_p->menu_item_chosen(TrayItem::receive_takeover);
  }
  else if (item_p == instance_p->receive_answer_item_p) {
    instance_p->menu_item_chosen(TrayItem::receive_answer);
  }
  else if (item_p == instance_p->receive_standby_item_p) {
    instance_p->menu_item_chosen(TrayItem::receive_standby);
  }
  else if (item_p == instance_p->stop_item_p) {
    instance_p->menu_item_chosen(TrayItem::stop);
  }
  else if (item_p == instance_p->quit_item_p) {
    instance_p->menu_item_chosen(TrayItem::quit);
  }
  else {
    write_error("Callback error in TrayItemCB::menuitem_activated_cb()\n");
  }
}

}  // anonymous namespace

TrayItem::TrayItem(GtkWindow* window_p): prog_window_p(window_p),
					 embedded(false) {

#ifdef HAVE_X11_XLIB_H

  menu_p = gtk_menu_new();
  GtkWidget* image_p;

  image_p = gtk_image_new_from_stock(GTK_STOCK_INDEX,  GTK_ICON_SIZE_MENU);
  list_received_faxes_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_label(gettext("List received faxes")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(list_received_faxes_item_p),
				image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_p), GTK_WIDGET(list_received_faxes_item_p));
  gtk_widget_show(GTK_WIDGET(list_received_faxes_item_p));

  image_p = gtk_image_new_from_stock(GTK_STOCK_INDEX,  GTK_ICON_SIZE_MENU);
  list_sent_faxes_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_label(gettext("List sent faxes")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(list_sent_faxes_item_p),
				image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_p), GTK_WIDGET(list_sent_faxes_item_p));
  gtk_widget_show(GTK_WIDGET(list_sent_faxes_item_p));

  // insert separator
  GtkWidget* separator_item_p = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_p), separator_item_p);
  gtk_widget_show(separator_item_p);

  receive_takeover_item_p =
    GTK_MENU_ITEM(gtk_menu_item_new_with_label(gettext("Take over call")));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_p), GTK_WIDGET(receive_takeover_item_p));
  gtk_widget_show(GTK_WIDGET(receive_takeover_item_p));

  receive_answer_item_p =
    GTK_MENU_ITEM(gtk_menu_item_new_with_label(gettext("Answer call")));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_p), GTK_WIDGET(receive_answer_item_p));
  gtk_widget_show(GTK_WIDGET(receive_answer_item_p));

  receive_standby_item_p =
    GTK_MENU_ITEM(gtk_menu_item_new_with_label(gettext("Receive standby")));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_p), GTK_WIDGET(receive_standby_item_p));
  gtk_widget_show(GTK_WIDGET(receive_standby_item_p));

  image_p = gtk_image_new_from_stock(GTK_STOCK_STOP,  GTK_ICON_SIZE_MENU);
  stop_item_p =
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_label(gettext("Stop")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(stop_item_p),
				image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_p), GTK_WIDGET(stop_item_p));
  gtk_widget_show(GTK_WIDGET(stop_item_p));

  // insert separator
  separator_item_p = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_p), separator_item_p);
  gtk_widget_show(separator_item_p);

  image_p = gtk_image_new_from_stock(GTK_STOCK_QUIT,  GTK_ICON_SIZE_MENU);
  quit_item_p = 
    GTK_MENU_ITEM(gtk_image_menu_item_new_with_label(gettext("Quit")));
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(quit_item_p),
				image_p);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_p), GTK_WIDGET(quit_item_p));
  gtk_widget_show(GTK_WIDGET(quit_item_p));

  g_signal_connect(G_OBJECT(list_received_faxes_item_p), "activate",
		   G_CALLBACK(TrayItemCB::menuitem_activated_cb), this);
  g_signal_connect(G_OBJECT(list_sent_faxes_item_p), "activate",
		   G_CALLBACK(TrayItemCB::menuitem_activated_cb), this);
  g_signal_connect(G_OBJECT(receive_takeover_item_p), "activate",
		   G_CALLBACK(TrayItemCB::menuitem_activated_cb), this);
  g_signal_connect(G_OBJECT(receive_answer_item_p), "activate",
		   G_CALLBACK(TrayItemCB::menuitem_activated_cb), this);
  g_signal_connect(G_OBJECT(receive_standby_item_p), "activate",
		   G_CALLBACK(TrayItemCB::menuitem_activated_cb), this);
  g_signal_connect(G_OBJECT(stop_item_p), "activate",
		   G_CALLBACK(TrayItemCB::menuitem_activated_cb), this);
  g_signal_connect(G_OBJECT(quit_item_p), "activate",
		   G_CALLBACK(TrayItemCB::menuitem_activated_cb), this);

  // now create and register the tray icon container
  event_box_p = gtk_event_box_new();
  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_scale_simple(prog_config.window_icon_h,
							   22, 22, GDK_INTERP_HYPER));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(event_box_p), image_p);
  }

  gtk_widget_add_events(event_box_p, GDK_BUTTON_PRESS_MASK);

  // set a tooltip
  tooltips_p = gtk_tooltips_new();
  // we have now initialised event_box_p and tooltips_p, so we can call set_tooltip_slot()
  set_tooltip_slot(gettext("Inactive"));
  
  g_signal_connect(G_OBJECT(event_box_p), "button_press_event",
		   G_CALLBACK(TrayItemCB::button_press_cb), this);

  tray_icon_p = egg_tray_icon_new("efax-gtk");

  g_signal_connect(G_OBJECT(tray_icon_p), "embedded",
		   G_CALLBACK(TrayItemCB::embedded_cb), this);
  g_signal_connect(G_OBJECT(tray_icon_p), "destroy",
		   G_CALLBACK(TrayItemCB::destroy_cb), this);

  gtk_container_add(GTK_CONTAINER(tray_icon_p), GTK_WIDGET(event_box_p));

  gtk_widget_show_all(GTK_WIDGET(tray_icon_p));

  // add a reference count to the tray icon container so that we that we keep
  // ownership of its destiny - we will need this for tray_icon_destroy() below
  g_object_ref(G_OBJECT(tray_icon_p));

#endif
}

void TrayItem::tray_icon_destroy(void) {

#ifdef HAVE_X11_XLIB_H

  // extract this event box from the defunct tray icon container
  // first we need to add a reference count to our event box so that
  // extracting it from the container doesn't destroy it
  g_object_ref(G_OBJECT(event_box_p));
  gtk_container_remove(GTK_CONTAINER(tray_icon_p), event_box_p);
  // now destroy the old tray icon
  g_object_unref(G_OBJECT(tray_icon_p));

  // create a new tray icon in case another GNOME notification area (system tray)
  // is inserted in the panel by the user, or the user has more than one tray
  tray_icon_p = egg_tray_icon_new("efax-gtk");
  gtk_container_add(GTK_CONTAINER(tray_icon_p), event_box_p);
  
  // now remove the additional reference count we added at the outset to the
  // the event box
  g_object_unref(G_OBJECT(event_box_p));

  g_signal_connect(G_OBJECT(tray_icon_p), "embedded",
		   G_CALLBACK(TrayItemCB::embedded_cb), this);
  g_signal_connect(G_OBJECT(tray_icon_p), "destroy",
		   G_CALLBACK(TrayItemCB::destroy_cb), this);

  gtk_widget_show_all(GTK_WIDGET(tray_icon_p));

  // add a reference count to the tray icon container so that we that we keep ownership
  // of its destiny - we will need this for any further calls to tray_icon_destroy()
  g_object_ref(G_OBJECT(tray_icon_p));

#endif
}

void TrayItem::set_tooltip_slot(const char* text) {

#ifdef HAVE_X11_XLIB_H

#ifdef HAVE_STRINGSTREAM
  std::ostringstream strm;
  strm << gettext("efax-gtk: ") << text;
  if (get_state() == EfaxController::receive_standby) {
    strm << '\n' << gettext("New faxes:") << ' ' << get_new_fax_count();
  }

  gtk_tooltips_set_tip(tooltips_p, event_box_p, strm.str().c_str(), "");
#else
  std::ostrstream strm;
  strm << gettext("efax-gtk: ") << text;
  if (get_state() == EfaxController::receive_standby) {
    strm << '\n' << gettext("New faxes:") << ' ' << get_new_fax_count();
  }

  strm << std::ends;
  const char* tip = strm.str();
  gtk_tooltips_set_tip(tooltips_p, event_box_p, tip, "");
  delete[] tip;
#endif

#endif
}
