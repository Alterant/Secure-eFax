/* Copyright (C) 2001 to 2007 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

// something in gtkprintjob.h clashes with the C++ headers with
// libstdc++-5 and it needs to be included here to obtain a
// clean compile
#include <gtk/gtkversion.h>
#if GTK_CHECK_VERSION(2,10,0)
#include <gtk/gtkprintjob.h>
#endif

#include <stdlib.h>  // include <stdlib.h> rather than <cstdlib> for mkstemp
#include <iomanip>
#include <memory>
#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

#include <gtk/gtkcontainer.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkbbox.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkpaned.h>
#include <gtk/gtkhpaned.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkmisc.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktoolbar.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkstock.h>

#include <gdk/gdkcolor.h>
#include <gdk/gdkpixbuf.h>
#include <gdk/gdkkeysyms.h> // the key codes are here

#include <glib/gtimer.h>

#include "fax_list.h"
#include "dialogs.h"
#include "fax_list_icons.h"
#include "utils/thread.h"
#include "utils/utf8_utils.h"
#include "utils/toolbar_append_widget.h"
#include "utils/callback.h"

#ifdef HAVE_OSTREAM
#include <ios>
#include <ostream>
#endif

#ifdef HAVE_STRINGSTREAM
#include <sstream>
#else
#include <strstream>
#endif

#ifdef HAVE_STREAM_IMBUE
#include <locale>
#endif

#if GTK_CHECK_VERSION(2,4,0)
#include <gtk/gtktoolitem.h>
#include <gtk/gtkseparatortoolitem.h>
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

int FaxListDialog::is_fax_received_list = 0;
int FaxListDialog::is_fax_sent_list = 0;

namespace { // callback for internal use only

void FaxListDialogCB::fax_list_button_clicked(GtkWidget* widget_p, void* data) {
  FaxListDialog* instance_p = static_cast<FaxListDialog*>(data);
  
  if (widget_p == instance_p->close_button_p) {
    instance_p->close();
  }
  else if (widget_p == instance_p->print_button_p) {
    instance_p->print_fax_prompt();
  }
  else if (widget_p == instance_p->view_button_p) {
    instance_p->view_fax();
  }
  else if (widget_p == instance_p->describe_button_p) {
    instance_p->describe_fax_prompt();
  }
  else if (widget_p == instance_p->delete_fax_button_p) {
    instance_p->delete_fax();
  }
  else if (widget_p == instance_p->empty_trash_button_p) {
    instance_p->empty_trash_prompt();
  }
  else if (widget_p == instance_p->add_folder_button_p) {
    instance_p->add_folder_prompt();
  }
  else if (widget_p == instance_p->delete_folder_button_p) {
    instance_p->delete_folder_prompt();
  }
  else if (widget_p == instance_p->reset_button_p) {
    instance_p->reset_sig();
    instance_p->display_new_fax_count();
  }
  else {
    write_error("Callback error in FaxListDialogCB::fax_list_book_button_clicked()\n");
    instance_p->close();
  }
}

}  // anonymous namespace

FaxListDialog::FaxListDialog(FaxListEnum::Mode mode_, const int standard_size_):
			      WinBase(0, prog_config.window_icon_h, false),
                              mode(mode_), standard_size(standard_size_),
			      working_dir(prog_config.working_dir),
                              fax_list_manager(mode) {

  // we have included a std::string working_dir member so that we can
  // access it in the FaxListDialog::print_fax_thread() without having
  // to introduce locking of accesses to prog_config.working_dir by
  // the main GUI thread.  It is fine to initialise it in the
  // initialisation list of this constructor as
  // prog_config.working_dir is only set once, on the first call to
  // configure_prog() (that is, when configure_prog() is passed false
  // as its argument)

  // notify the existence of this object
  if (mode == FaxListEnum::received) is_fax_received_list++;
  else is_fax_sent_list++;

  close_button_p = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
  print_button_p = gtk_button_new();
  view_button_p = gtk_button_new();
  describe_button_p = gtk_button_new();
  delete_fax_button_p = gtk_button_new();
  empty_trash_button_p = gtk_button_new();
  add_folder_button_p = gtk_button_new();
  delete_folder_button_p = gtk_button_new();

  GtkWidget* button_box_p = gtk_hbutton_box_new();
  GtkWidget* fax_list_box_p = gtk_vbox_new(false, 0);
  GtkWidget* list_pane_p = gtk_hpaned_new();
  GtkTable* table_p = GTK_TABLE(gtk_table_new(2, 1, false));
  GtkScrolledWindow* folder_list_scrolled_window_p
    = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(0, 0));
  GtkScrolledWindow* fax_list_scrolled_window_p
    = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(0, 0));
  GtkToolbar* toolbar_p = (GTK_TOOLBAR(gtk_toolbar_new()));

  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box_p), GTK_BUTTONBOX_SPREAD);

  gtk_scrolled_window_set_shadow_type(folder_list_scrolled_window_p, GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy(folder_list_scrolled_window_p,
				 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  fax_list_manager.insert_folder_tree_view(GTK_CONTAINER(folder_list_scrolled_window_p));

  gtk_scrolled_window_set_shadow_type(fax_list_scrolled_window_p, GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy(fax_list_scrolled_window_p,
				 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  fax_list_manager.insert_fax_tree_view(GTK_CONTAINER(fax_list_scrolled_window_p));

  // set up the tool bar
  gtk_toolbar_set_orientation(toolbar_p, GTK_ORIENTATION_HORIZONTAL);
  gtk_toolbar_set_style(toolbar_p, GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_tooltips(toolbar_p, true);

  // is this necessary after calling gtk_toolbar_set_tooltips()?
  // this is OK as tooltips is a public member of GtkToolbar
  gtk_tooltips_enable(toolbar_p->tooltips);

  // bring up the icon size registered in MainWindow::MainWindow()
  GtkIconSize efax_gtk_button_size = gtk_icon_size_from_name("EFAX_GTK_BUTTON_SIZE");

  GtkWidget* image_p;
  image_p = gtk_image_new_from_stock(GTK_STOCK_PRINT, efax_gtk_button_size);
  gtk_container_add(GTK_CONTAINER(print_button_p), image_p);
  toolbar_append_widget(toolbar_p, print_button_p,
			gettext("Print selected fax"), "");

  gtk_widget_set_sensitive(print_button_p, false);
  gtk_button_set_relief(GTK_BUTTON(print_button_p), GTK_RELIEF_NONE);

  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(view_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(view_button_p), image_p);
  }
  toolbar_append_widget(toolbar_p, view_button_p,
			gettext("View selected fax"), "");
  gtk_widget_set_sensitive(view_button_p, false);
  gtk_button_set_relief(GTK_BUTTON(view_button_p), GTK_RELIEF_NONE);

  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(describe_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(describe_button_p), image_p);
  }
  toolbar_append_widget(toolbar_p, describe_button_p,
			gettext("Add/amend description for selected fax"), "");
  gtk_widget_set_sensitive(describe_button_p, false);
  gtk_button_set_relief(GTK_BUTTON(describe_button_p), GTK_RELIEF_NONE);

  image_p = gtk_image_new_from_stock(GTK_STOCK_DELETE, efax_gtk_button_size);
  gtk_container_add(GTK_CONTAINER(delete_fax_button_p), image_p);
  toolbar_append_widget(toolbar_p, delete_fax_button_p,
			gettext("Delete selected fax"), "");
  gtk_widget_set_sensitive(delete_fax_button_p, false);
  gtk_button_set_relief(GTK_BUTTON(delete_fax_button_p), GTK_RELIEF_NONE);

  image_p = gtk_image_new_from_stock(GTK_STOCK_CLEAR, efax_gtk_button_size);
  gtk_container_add(GTK_CONTAINER(empty_trash_button_p), image_p);
  toolbar_append_widget(toolbar_p, empty_trash_button_p,
			gettext("Empty trash folder"), "");
  gtk_widget_set_sensitive(empty_trash_button_p, false);
  gtk_button_set_relief(GTK_BUTTON(empty_trash_button_p), GTK_RELIEF_NONE);

  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(add_folder_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(add_folder_button_p), image_p);
  }
  toolbar_append_widget(toolbar_p, add_folder_button_p,
			gettext("Add new folder"), "");
  gtk_widget_set_sensitive(add_folder_button_p, true);
  gtk_button_set_relief(GTK_BUTTON(add_folder_button_p), GTK_RELIEF_NORMAL);

  { // scope block for the GobjHandle
    // GdkPixbufs are not owned by a GTK+ container when passed to it so use a GobjHandle
    GobjHandle<GdkPixbuf> pixbuf_h(gdk_pixbuf_new_from_xpm_data(delete_folder_xpm));
    image_p = gtk_image_new_from_pixbuf(pixbuf_h);
    gtk_container_add(GTK_CONTAINER(delete_folder_button_p), image_p);
  }
  toolbar_append_widget(toolbar_p, delete_folder_button_p,
			gettext("Delete selected folder"), "");
  gtk_widget_set_sensitive(delete_folder_button_p, false);
  gtk_button_set_relief(GTK_BUTTON(delete_folder_button_p), GTK_RELIEF_NONE);

  gtk_container_add(GTK_CONTAINER(button_box_p), close_button_p);

  gtk_paned_pack1(GTK_PANED(list_pane_p),
		  GTK_WIDGET(folder_list_scrolled_window_p),
		  false, true);

  gtk_paned_pack2(GTK_PANED(list_pane_p),
		  GTK_WIDGET(fax_list_scrolled_window_p),
		  true, true);

  gtk_paned_set_position(GTK_PANED(list_pane_p), standard_size * 5);

  gtk_table_attach(table_p, list_pane_p,
		   0, 1, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL | GTK_EXPAND),
		   standard_size/3, standard_size/3);

  gtk_table_attach(table_p, button_box_p,
		   0, 1, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/3, standard_size/3);

  g_signal_connect(G_OBJECT(close_button_p), "clicked",
		   G_CALLBACK(FaxListDialogCB::fax_list_button_clicked), this);
  g_signal_connect(G_OBJECT(print_button_p), "clicked",
		   G_CALLBACK(FaxListDialogCB::fax_list_button_clicked), this);
  g_signal_connect(G_OBJECT(view_button_p), "clicked",
		   G_CALLBACK(FaxListDialogCB::fax_list_button_clicked), this);
  g_signal_connect(G_OBJECT(describe_button_p), "clicked",
		   G_CALLBACK(FaxListDialogCB::fax_list_button_clicked), this);
  g_signal_connect(G_OBJECT(delete_fax_button_p), "clicked",
		   G_CALLBACK(FaxListDialogCB::fax_list_button_clicked), this);
  g_signal_connect(G_OBJECT(empty_trash_button_p), "clicked",
		   G_CALLBACK(FaxListDialogCB::fax_list_button_clicked), this);
  g_signal_connect(G_OBJECT(add_folder_button_p), "clicked",
		   G_CALLBACK(FaxListDialogCB::fax_list_button_clicked), this);
  g_signal_connect(G_OBJECT(delete_folder_button_p), "clicked",
		   G_CALLBACK(FaxListDialogCB::fax_list_button_clicked), this);

  GtkWidget* toolbar_box_p = gtk_hbox_new(false, 0);
  gtk_box_pack_start(GTK_BOX(toolbar_box_p), GTK_WIDGET(toolbar_p), true, true, 0);

  GtkWidget* new_fax_frame_p = 0;
  // if mode == FaxListEnum::sent we don't do anything at the moment
  if (mode == FaxListEnum::received) {

    // first provide a reset button
    reset_button_p = gtk_button_new();
    image_p = gtk_image_new_from_stock(GTK_STOCK_UNDO, efax_gtk_button_size);
    gtk_container_add(GTK_CONTAINER(reset_button_p), image_p);
    toolbar_append_widget(toolbar_p, reset_button_p,
			  gettext("Reset new faxes count"), "");
    gtk_widget_set_sensitive(reset_button_p, true);
    gtk_button_set_relief(GTK_BUTTON(reset_button_p), GTK_RELIEF_NORMAL);
    g_signal_connect(G_OBJECT(reset_button_p), "clicked",
		     G_CALLBACK(FaxListDialogCB::fax_list_button_clicked), this);

    // now provide a display for the new received fax count
    new_fax_frame_p = gtk_frame_new(0);
    gtk_container_set_border_width(GTK_CONTAINER(new_fax_frame_p), 1);
    new_fax_label_p = gtk_label_new(0);
    gtk_misc_set_padding(GTK_MISC(new_fax_label_p), 6, 2);
    gtk_container_add(GTK_CONTAINER(new_fax_frame_p), new_fax_label_p);
    gtk_label_set_justify(GTK_LABEL(new_fax_label_p), GTK_JUSTIFY_RIGHT);

#if GTK_CHECK_VERSION(2,4,0)
    GtkToolItem* separator_p = gtk_separator_tool_item_new();
    gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(separator_p), false);
    gtk_tool_item_set_expand(separator_p, true);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar_p), separator_p, -1);
    toolbar_append_widget(GTK_TOOLBAR(toolbar_p), new_fax_frame_p, "", "");
#else
    GtkWidget* new_fax_toolbar_p = gtk_toolbar_new();
    gtk_box_pack_end(GTK_BOX(toolbar_box_p), new_fax_toolbar_p, false, false, 0);
    toolbar_append_widget(GTK_TOOLBAR(new_fax_toolbar_p), new_fax_frame_p, "", "");
#endif

    // as this is in the constructor, get_new_fax_count() always shows a 0 count
    // but this is OK as we are using it here to size the label - the user must
    // call display_new_fax_count() again once get_new_fax_count_sig has been
    // connected up
    display_new_fax_count();
  }
  else reset_button_p = 0;

  // now put the toolbar hbox and the main table into the window vbox
  gtk_box_pack_start(GTK_BOX(fax_list_box_p), GTK_WIDGET(toolbar_box_p), false, false, 0);
  gtk_box_pack_start(GTK_BOX(fax_list_box_p), GTK_WIDGET(table_p), true, true, 0);
  
  // now connect up the signal which indicates a selection has been made
  fax_list_manager.selection_notify.connect(sigc::mem_fun(*this, &FaxListDialog::set_buttons_slot));

  GTK_WIDGET_SET_FLAGS(close_button_p, GTK_CAN_DEFAULT);
  gtk_widget_grab_focus(close_button_p);

  gtk_container_set_border_width(GTK_CONTAINER(table_p), standard_size/3);

  if (mode == FaxListEnum::received) gtk_window_set_title(get_win(),
							  gettext("efax-gtk: Received fax list"));
  else gtk_window_set_title(get_win(), gettext("efax-gtk: Sent fax list"));

  //gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER);

  gtk_container_add(GTK_CONTAINER(get_win()), GTK_WIDGET(fax_list_box_p));

  gtk_window_set_default_size(get_win(), standard_size * 18, standard_size * 16);
#if GTK_CHECK_VERSION(2,4,0)
  gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar_p), false);
#endif
  gtk_widget_show_all(GTK_WIDGET(get_win()));

  // not needed if gtk_toolbar_set_show_arrow() has been set false, or GTK+ version < 2.4
  /*
  if (mode == FaxListEnum::received) {
    gtk_widget_set_size_request(GTK_WIDGET(toolbar_p),
				print_button_p->allocation.width + view_button_p->allocation.width
				+ describe_button_p->allocation.width + delete_fax_button_p->allocation.width
				+ empty_trash_button_p->allocation.width + add_folder_button_p->allocation.width
				+ delete_folder_button_p->allocation.width + reset_button_p->allocation.width
				+ new_fax_frame_p->allocation.width + 16,
				print_button_p->allocation.height);
  }
  else {
    gtk_widget_set_size_request(GTK_WIDGET(toolbar_p),
				print_button_p->allocation.width + view_button_p->allocation.width
				+ describe_button_p->allocation.width + delete_fax_button_p->allocation.width
				+ empty_trash_button_p->allocation.width + add_folder_button_p->allocation.width
				+ delete_folder_button_p->allocation.width + 12,
				print_button_p->allocation.height);
  }
  */
}

FaxListDialog::~FaxListDialog(void) {

  std::vector<SharedHandle<char*> >::const_iterator iter;
  for (iter = view_files.begin(); iter != view_files.end(); ++iter) {
    unlink(iter->get());
  }
  // notify the destruction of this object
  if (mode == FaxListEnum::received) is_fax_received_list--;
  else is_fax_sent_list--;
}

void FaxListDialog::set_buttons_slot(void) {

  // see if fax is selected
  RowPathList::size_type fax_count = fax_list_manager.is_fax_selected();

  if (fax_count) {
    gtk_widget_set_sensitive(delete_fax_button_p, true);
    gtk_button_set_relief(GTK_BUTTON(delete_fax_button_p), GTK_RELIEF_NORMAL);
  }
  else {
    gtk_widget_set_sensitive(delete_fax_button_p, false);
    gtk_button_set_relief(GTK_BUTTON(delete_fax_button_p), GTK_RELIEF_NONE);
  }

  if (fax_count == 1) {
    gtk_widget_set_sensitive(print_button_p, true);
    gtk_widget_set_sensitive(view_button_p, true);
    gtk_widget_set_sensitive(describe_button_p, true);

    gtk_button_set_relief(GTK_BUTTON(print_button_p), GTK_RELIEF_NORMAL);
    gtk_button_set_relief(GTK_BUTTON(view_button_p), GTK_RELIEF_NORMAL);
    gtk_button_set_relief(GTK_BUTTON(describe_button_p), GTK_RELIEF_NORMAL);
  }
  else {
    gtk_widget_set_sensitive(print_button_p, false);
    gtk_widget_set_sensitive(view_button_p, false);
    gtk_widget_set_sensitive(describe_button_p, false);

    gtk_button_set_relief(GTK_BUTTON(print_button_p), GTK_RELIEF_NONE);
    gtk_button_set_relief(GTK_BUTTON(view_button_p), GTK_RELIEF_NONE);
    gtk_button_set_relief(GTK_BUTTON(describe_button_p), GTK_RELIEF_NONE);
  }

  // see if folder is selected
  if (fax_list_manager.is_folder_selected()) {
    if (!fax_list_manager.is_selected_folder_permanent()) {
      gtk_widget_set_sensitive(delete_folder_button_p, true);
      gtk_button_set_relief(GTK_BUTTON(delete_folder_button_p), GTK_RELIEF_NORMAL);
    }
    else {
      gtk_widget_set_sensitive(delete_folder_button_p, false);
      gtk_button_set_relief(GTK_BUTTON(delete_folder_button_p), GTK_RELIEF_NONE);
    }
    if (fax_list_manager.show_trash_folder_icon()) {
      gtk_widget_set_sensitive(empty_trash_button_p, true);
      gtk_button_set_relief(GTK_BUTTON(empty_trash_button_p), GTK_RELIEF_NORMAL);
    }
    else {
      gtk_widget_set_sensitive(empty_trash_button_p, false);
      gtk_button_set_relief(GTK_BUTTON(empty_trash_button_p), GTK_RELIEF_NONE);
    }
  }
}

void FaxListDialog::delete_fax(void) {

  RowPathList::size_type fax_count;
  if ((fax_count = fax_list_manager.is_fax_selected())) {

    if (fax_list_manager.are_selected_faxes_in_trash_folder()) {

      std::string msg;

      if (fax_count == 1) {
	msg = gettext("Permanently delete selected fax?\n");
	msg += gettext("\n(NOTE: This will permanently delete the fax\n"
		       "from the file system)");
      }
      else {
	msg = gettext("Permanently delete selected faxes?\n"
		      "\n(NOTE: This will permanently delete the faxes\n"
		       "from the file system)");
      }

      PromptDialog* dialog_p = new PromptDialog(msg.c_str(), gettext("efax-gtk: Delete fax"),
						standard_size, get_win());
      dialog_p->accepted.connect(sigc::mem_fun(fax_list_manager, &FaxListManager::delete_fax));
      // there is no memory leak -- the memory will be deleted when PromptDialog closes
    }
    else fax_list_manager.move_selected_faxes_to_trash_folder();
  }
}

void FaxListDialog::describe_fax_prompt(void) {

  if (fax_list_manager.is_fax_selected() == 1) {
    GcharSharedHandle fax_description_h(fax_list_manager.get_fax_description());
    // fax_description_h will be null if there is no fax description already
    std::string description;
    if (fax_description_h.get()) description = fax_description_h.get();

    DescriptionDialog* dialog_p = new DescriptionDialog(standard_size,
							description.c_str(),
							get_win());
    dialog_p->accepted.connect(sigc::mem_fun(fax_list_manager, &FaxListManager::describe_fax));
    // there is no memory leak -- the memory will be deleted when DescriptionDialog closes
  }
}

void FaxListDialog::empty_trash_prompt(void) {

  std::string msg(gettext("Empty trash folder?\n"
			  "\n(NOTE: This will permanently delete all the faxes\n"
			  "in the Trash folder from the file system)"));
  PromptDialog* dialog_p = new PromptDialog(msg.c_str(), gettext("efax-gtk: Trash folder"),
					    standard_size, get_win());
  dialog_p->accepted.connect(sigc::mem_fun(fax_list_manager, &FaxListManager::empty_trash_folder));
  // there is no memory leak -- the memory will be deleted when PromptDialog closes
}

void FaxListDialog::add_folder_prompt(void) {

  AddFolderDialog* dialog_p = new AddFolderDialog(standard_size, get_win());
  dialog_p->accepted.connect(sigc::mem_fun(*this, &FaxListDialog::add_folder));
  // there is no memory leak -- the memory will be deleted when AddFolderDialog closes

}

void FaxListDialog::add_folder(const std::string& folder_name) {

  std::pair<bool, std::string> result = fax_list_manager.is_folder_name_valid(folder_name);
  if (!result.first) {
    new InfoDialog(result.second.c_str(), 
		   gettext("efax-gtk: Add folder"),
		   GTK_MESSAGE_WARNING,
		   get_win());
    // there is no memory leak -- the memory will be deleted when InfoDialog closes
  }    

  else fax_list_manager.make_folder(folder_name, false);
}

void FaxListDialog::delete_folder_prompt(void) {

  // the "Delete folder" button will not be sensitive if
  // fax_list_manager.is_selected_folder_permanent() returns true
  // but test it here again just in case
  if (fax_list_manager.is_selected_folder_permanent()) beep();

  else if (fax_list_manager.is_folder_selected()) {

    if (!fax_list_manager.is_selected_folder_empty()) {
      new InfoDialog(gettext("Empty the folder by deleting any child folders\n"
                             "and drag-and-dropping or deleting any contained\n"
			     "faxes before deleting folder"), 
		     gettext("efax-gtk: Delete folder"),
		     GTK_MESSAGE_WARNING,
		     get_win());
      // there is no memory leak -- the memory will be deleted when InfoDialog closes
    }

    else {
      GcharSharedHandle folder_name_h(fax_list_manager.get_folder_name());
      std::string msg(gettext("Delete folder: "));
      // as we checked for a folder selection, folder_name_h should not be
      // null, but check it in case
      if (folder_name_h.get()) msg += folder_name_h.get();
      msg += gettext("?");

      PromptDialog* dialog_p = new PromptDialog(msg.c_str(), gettext("efax-gtk: Delete folder"),
						standard_size, get_win());
      dialog_p->accepted.connect(sigc::mem_fun(fax_list_manager, &FaxListManager::delete_folder));
      // there is no memory leak -- the memory will be deleted when PromptDialog closes
    }
  }
}

void FaxListDialog::print_fax_prompt(void) {

  // we do not need a mutex before reading prog_config.print_cmd
  // as although read in FaxListDialog::get_print_from_stdin_parms(),
  // the thread which executes that method cannot start until the prompt
  // dialog below is launched
  if(fax_list_manager.is_fax_selected() == 1
     && !prog_config.print_cmd.empty()) {
    // we do not need a mutex before reading prog_config.print_popup either, even though
    // it is also read in FaxListDialog::print_fax_thread(), because it is of type
    // bool which can be safely concurrently read in different threads and in any
    // event it is read in FaxListDialog::print_fax_thread() in a separate process
    // after fork()ing so there isn't even concurrent access
#if GTK_CHECK_VERSION(2,10,0)
    if (prog_config.gtkprint) print_fax();
    else if (prog_config.print_popup) {
#else
    if (prog_config.print_popup) {
#endif      
      std::string msg(gettext("Print selected fax?"));
      PromptDialog* dialog_p = new PromptDialog(msg.c_str(), gettext("efax-gtk: Print fax"),
						standard_size, get_win());
      dialog_p->accepted.connect(sigc::mem_fun(*this, &FaxListDialog::print_fax));
      // there is no memory leak -- the memory will be deleted when PromptDialog closes
    }
    else print_fax();
  }
}

std::pair<const char*, char* const*> FaxListDialog::get_print_from_stdin_parms(void) {

  std::vector<std::string> print_parms;
  std::string print_cmd;
  std::string print_name;
  std::string::size_type end_pos;

  { // scope block for mutex lock
    // lock the Prog_config object to stop it being modified or accessed in the initial (GUI)
    // thread while we are accessing it here
    Thread::Mutex::Lock lock(*prog_config.mutex_p);

    try {
      print_cmd = Utf8::filename_from_utf8(prog_config.print_cmd);
    }
    catch (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in FaxListDialog::print_from_stdin()\n");
      return std::pair<const char*, char* const*>(0,0);
    }
  }
  
  if ((end_pos = print_cmd.find_first_of(' ')) != std::string::npos) { // we have parms
    print_name.assign(print_cmd, 0, end_pos);
    print_parms.push_back(print_name);
    // find start of next parm
    std::string::size_type start_pos = print_cmd.find_first_not_of(' ', end_pos);
    while (start_pos != std::string::npos) {
      end_pos = print_cmd.find_first_of(' ', start_pos);
      if (end_pos != std::string::npos) {
	print_parms.push_back(print_cmd.substr(start_pos, end_pos - start_pos));
	start_pos = print_cmd.find_first_not_of(' ', end_pos); // prepare for next interation
      }
      else {
	print_parms.push_back(print_cmd.substr(start_pos, 
					       print_cmd.size() - start_pos));
	start_pos = end_pos;
      }
    }
  }

  else { // just a print command without parameters to be passed
    print_name = print_cmd;
    print_parms.push_back(print_name);
  }

  char** exec_parms = new char*[print_parms.size() + 1];

  char** temp_pp = exec_parms;
  std::vector<std::string>::const_iterator iter;
  for (iter = print_parms.begin(); iter != print_parms.end(); ++iter, ++temp_pp) {
    *temp_pp = new char[iter->size() + 1];
    std::strcpy(*temp_pp, iter->c_str());
  }
  
  *temp_pp = 0;

  char* prog_name = new char[print_name.size() + 1];
  std::strcpy(prog_name, print_name.c_str());

  return std::pair<const char*, char* const*>(prog_name, exec_parms);
}

std::pair<const char*, char* const*> FaxListDialog::get_fax_to_ps_parms
                                       (const std::string& basename, bool allow_shrink) {

  // set up the parms for efix
  std::vector<std::string> efix_parms;
  std::string temp;
    
  { // scope block for mutex lock

    // lock the Prog_config object to stop it being modified or accessed in the initial (GUI)
    // thread while we are accessing it here
    Thread::Mutex::Lock lock(*prog_config.mutex_p);

    efix_parms.push_back("efix-0.9a");
    // shut up efix (comment out next line and uncomment following one if errors to be reported)
    efix_parms.push_back("-v");
    //efix_parms.push_back("-ve");
    efix_parms.push_back("-r300");
    efix_parms.push_back("-ops");
    temp = "-p";
    temp += prog_config.page_dim;
    efix_parms.push_back(temp);

#ifdef HAVE_STRINGSTREAM
    if (allow_shrink && prog_config.print_shrink.compare("100")) { // if print_shrink is not 100
      temp = "-s0.";
      temp += prog_config.print_shrink;
      efix_parms.push_back(temp);

      std::ostringstream strm;

#  ifdef HAVE_STREAM_IMBUE
      strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

      if (!prog_config.page_size.compare("a4")) {

	strm << "-d";
	float val = 210 * (100 - std::atoi(prog_config.print_shrink.c_str()))/200.0;
	strm << static_cast<int>(val + 0.5);
	val = 297 * (100 - std::atoi(prog_config.print_shrink.c_str()))/200.0;
	strm << ',' << static_cast<int>(val + 0.5) << "mm";
      
	efix_parms.push_back(strm.str());
      }

      else if (!prog_config.page_size.compare("letter")) {

	strm << "-d";
	float val = 216 * (100 - std::atoi(prog_config.print_shrink.c_str()))/200.0;
	strm << static_cast<int>(val + 0.5);
	val = 279 * (100 - std::atoi(prog_config.print_shrink.c_str()))/200.0;
	strm << ',' << static_cast<int>(val + 0.5) << "mm";

	efix_parms.push_back(strm.str());
      }

      else if (!prog_config.page_size.compare("legal")) {

	strm << "-d";
	float val = 216 * (100 - std::atoi(prog_config.print_shrink.c_str()))/200.0;
	strm << static_cast<int>(val + 0.5);
	val = 356 * (100 - std::atoi(prog_config.print_shrink.c_str()))/200.0;
	strm << ',' << static_cast<int>(val + 0.5) << "mm";

	efix_parms.push_back(strm.str());
      }
    }

    int partnumber = 1;
    std::ostringstream strm;

#  ifdef HAVE_STREAM_IMBUE
    strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

    strm << basename.c_str() << std::setfill('0')
	 << std::setw(3) << partnumber;
    int result = access(strm.str().c_str(), R_OK);

    while (!result) {
      efix_parms.push_back(strm.str());
	
      partnumber++;
      strm.str("");
      strm << basename.c_str() << std::setfill('0')
	   << std::setw(3) << partnumber;
      result = access(strm.str().c_str(), R_OK);
    }

#else
    if (allow_shrink && prog_config.print_shrink.compare("100")) { // if print_shrink is not 100
      temp = "-s0.";
      temp += prog_config.print_shrink;
      efix_parms.push_back(temp);

      std::ostrstream strm;

#  ifdef HAVE_STREAM_IMBUE
      strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

      if (!prog_config.page_size.compare("a4")) {

	strm << "-d";
	float val = 210 * (100 - std::atoi(prog_config.print_shrink.c_str()))/200.0;
	strm << static_cast<int>(val + 0.5);
	val = 297 * (100 - std::atoi(prog_config.print_shrink.c_str()))/200.0;
	strm << ',' << static_cast<int>(val + 0.5) << "mm" << std::ends;

	const char* dim = strm.str();
	efix_parms.push_back(dim);
	delete[] dim;
      }
    
      else if (!prog_config.page_size.compare("letter")) {

	strm << "-d";
	float val = 216 * (100 - std::atoi(prog_config.print_shrink.c_str()))/200.0;
	strm << static_cast<int>(val + 0.5);
	val = 279 * (100 - std::atoi(prog_config.print_shrink.c_str()))/200.0;
	strm << ',' << static_cast<int>(val + 0.5) << "mm" << std::ends;

	const char* dim = strm.str();
	efix_parms.push_back(dim);
	delete[] dim;
      }

      else if (!prog_config.page_size.compare("legal")) {

	strm << "-d";
	float val = 216 * (100 - std::atoi(prog_config.print_shrink.c_str()))/200.0;
	strm << static_cast<int>(val + 0.5);
	val = 356 * (100 - std::atoi(prog_config.print_shrink.c_str()))/200.0;
	strm << ',' << static_cast<int>(val + 0.5) << "mm" << std::ends;

      const char* dim = strm.str();
      efix_parms.push_back(dim);
      delete[] dim;
      }
    }

    int partnumber = 1;
    std::ostrstream strm;

#  ifdef HAVE_STREAM_IMBUE
    strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

    strm << basename.c_str() << std::setfill('0')
	 << std::setw(3) << partnumber << std::ends;
    const char* file_name = strm.str();
    int result = access(file_name, R_OK);

    while (!result) {
      efix_parms.push_back(file_name);
      delete[] file_name;

      partnumber++;
      std::ostrstream strm;

#  ifdef HAVE_STREAM_IMBUE
      strm.imbue(std::locale::classic());
#  endif // HAVE_STREAM_IMBUE

      strm << basename.c_str() << std::setfill('0')
	   << std::setw(3) << partnumber << std::ends;
      file_name = strm.str();
      result = access(file_name, R_OK);
    }
    delete[] file_name;
#endif
  } // end of mutex lock scope block

  char** exec_parms = new char*[efix_parms.size() + 1];

  char** temp_pp = exec_parms;
  std::vector<std::string>::const_iterator iter;
  for (iter = efix_parms.begin(); iter != efix_parms.end(); ++iter, ++temp_pp) {
    *temp_pp = new char[iter->size() + 1];
    std::strcpy(*temp_pp, iter->c_str());
  }
  
  *temp_pp = 0;

  char* prog_name = new char[std::strlen("efix-0.9a") + 1];
  std::strcpy(prog_name, "efix-0.9a");

  return std::pair<const char*, char* const*>(prog_name, exec_parms);
}

void FaxListDialog::write_pipe_to_file(PipeFifo* pipe_p, int file_fd) {

  ssize_t read_result;
  ssize_t write_result;
  ssize_t written;
  char buffer[PIPE_BUF];

  while ((read_result = pipe_p->read(buffer, PIPE_BUF)) > 0) {
    written = 0;
    do {
      write_result = write(file_fd, buffer + written, read_result);
      if (write_result > 0) {
	written += write_result;
	read_result -= write_result;
      }
    } while (read_result && (write_result != -1 || errno == EINTR));
  }
  g_usleep(50000);
}

std::pair<const char*, char* const*> FaxListDialog::get_ps_viewer_parms(const char* filename) {

  std::vector<std::string> view_parms;
  std::string view_cmd;
  std::string view_name;
  std::string::size_type end_pos;

  { // scope block for mutex lock

    // lock the Prog_config object to stop it being modified or accessed in the initial (GUI)
    // thread while we are accessing it here
    Thread::Mutex::Lock lock(*prog_config.mutex_p);
    try {
      view_cmd = Utf8::filename_from_utf8(prog_config.ps_view_cmd);
    }
    catch (Utf8::ConversionError&) {
      write_error("UTF-8 conversion error in FaxListDialog::ps_viewer()\n");
      return std::pair<const char*, char* const*>(0,0);
    }
  }

  if ((end_pos = view_cmd.find_first_of(' ')) != std::string::npos) { // we have parms
    view_name.assign(view_cmd, 0, end_pos);
    view_parms.push_back(view_name);
    // find start of next parm
    std::string::size_type start_pos = view_cmd.find_first_not_of(' ', end_pos);
    while (start_pos != std::string::npos) {
      end_pos = view_cmd.find_first_of(' ', start_pos);
      if (end_pos != std::string::npos) {
	view_parms.push_back(view_cmd.substr(start_pos, end_pos - start_pos));
	start_pos = view_cmd.find_first_not_of(' ', end_pos); // prepare for next interation
      }
      else {
	view_parms.push_back(view_cmd.substr(start_pos, 
					     view_cmd.size() - start_pos));
	start_pos = end_pos;
      }
    }
  }

  else { // just a print command without parameters to be passed
    view_name = view_cmd;
    view_parms.push_back(view_name);
  }

  view_parms.push_back(filename);

  char** exec_parms = new char*[view_parms.size() + 1];

  char** temp_pp = exec_parms;
  std::vector<std::string>::const_iterator iter;
  for (iter = view_parms.begin(); iter != view_parms.end(); ++iter, ++temp_pp) {
    *temp_pp = new char[iter->size() + 1];
    std::strcpy(*temp_pp, iter->c_str());
  }
  
  *temp_pp = 0;

  char* prog_name = new char[view_name.size() + 1];
  std::strcpy(prog_name, view_name.c_str());

  return std::pair<const char*, char* const*>(prog_name, exec_parms);
}

void FaxListDialog::print_fax(void) {

  if (fax_list_manager.is_fax_selected() == 1 && !prog_config.print_cmd.empty()) {

    std::auto_ptr<std::string> fax_basename_a(new std::string(prog_config.working_dir));
    if (mode == FaxListEnum::received) *fax_basename_a += "/faxin/";
    else *fax_basename_a += "/faxsent/";

    // we don't need to use a Glib conversion function here - we know the
    // fax name is just plain ASCII numbers
    GcharSharedHandle fax_number_h(fax_list_manager.get_fax_number());
    std::string fax_number;
    // as we checked for a fax selection, fax_number_h should not be
    // null, but check it in case
    if (fax_number_h.get()) fax_number = fax_number_h.get();
    *fax_basename_a += fax_number;
    *fax_basename_a += '/';
    *fax_basename_a += fax_number;
    *fax_basename_a += '.';

    // create the print manager
#if GTK_CHECK_VERSION(2,10,0)
    if (prog_config.gtkprint) async_queue.push(FilePrintManager::create_manager(get_win(),
										gettext("efax-gtk: Print fax"),
										prog_config.window_icon_h));
#endif

    // now block off the signals for which we have set handlers so that the worker
    // thread does not receive the signals, otherwise we will have memory synchronisation
    // issues in multi-processor systems - we will unblock in the initial (GUI) thread
    // as soon as the socket server thread has been launched
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGCHLD);
    sigaddset(&sig_mask, SIGQUIT);
    sigaddset(&sig_mask, SIGTERM);
    sigaddset(&sig_mask, SIGINT);
    sigaddset(&sig_mask, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &sig_mask, 0);

    // hand ownership of fax_basename_a to the new thread - Thread::Thread::start()
    // could throw in the event of memory allocation failure, as could the creation of
    // the Callback2 object on free store, but as a matter of policy we do not deal
    // with that condition in this program so we can pass a raw pointer rather than
    // the std::auto_ptr<> object - anyway passing a std::auto_ptr<> object wouldn't
    // be enough as we would still have to catch the std::bad_alloc exception in
    // order to pop() off the object in async_queue
    std::string* arg_p = fax_basename_a.release();
    std::auto_ptr<Thread::Thread> res =
      Thread::Thread::start(Callback::make(*this, &FaxListDialog::print_fax_thread,
					   arg_p, prog_config.gtkprint),
			    false);
    if (!res.get()) {
      delete arg_p;
      write_error("Cannot start new print thread, fax will not be printed\n");
#if GTK_CHECK_VERSION(2,10,0)
      // Because another thread (for another print job) could have been
      // launched this may extract a different print manager object from
      // the one created above in this thread, but that doesn't matter as
      // they are all the same - the filename passed as the argument to
      // print_fax_thread() which in fact determines the print job printed
      if (prog_config.gtkprint) {
	try {
	  async_queue.pop();
	}
	catch (AsyncQueuePopError& error) {
	  write_error(error.what());
	  write_error(" in FaxListDialog::print_fax()\n");
	}
      }
#endif
    }
    // now unblock the signals so that the initial (GUI) thread can receive them
    pthread_sigmask(SIG_UNBLOCK, &sig_mask, 0);
  }
}

void FaxListDialog::print_fax_thread(std::string* arg_p, bool use_gtkprint) {

  // extract the string argument to a std::auto_ptr<> object
  const std::auto_ptr<std::string> fax_basename_a(arg_p);

  // extract any FilePrintManager object
#if GTK_CHECK_VERSION(2,10,0)
  IntrusivePtr<FilePrintManager> print_manager_i;
  if (use_gtkprint) {
    try {
     async_queue.pop(print_manager_i);
    }
    catch (AsyncQueuePopError& error) {
      write_error(error.what());
      write_error(" in FaxListDialog::print_fax_thread()\n");
      return;
    }
  }
#endif
  // get the arguments for the exec() calls below (because this is a
  // multi- threaded program, we must do this before fork()ing because
  // we use functions to get the arguments which are not async-signal-safe)
  std::pair<const char*, char* const*> fax_to_ps_parms(get_fax_to_ps_parms(*fax_basename_a, true));

#if GTK_CHECK_VERSION(2,10,0)
  if (use_gtkprint) {

    // get a temporary file
    std::string filename;
    {
      Thread::Mutex::Lock lock(working_dir_mutex);
      filename = working_dir;
    }
    filename += "/efax-gtk-print-XXXXXX";

    std::string::size_type size = filename.size() + 1;
    ScopedHandle<char*> tempfile_h(new char[size]);
    std::memcpy(tempfile_h.get(), filename.c_str(), size); // this will include the terminating '\0' in the copy
    int file_fd = mkstemp(tempfile_h.get());

    if (file_fd == -1) {
      write_error("Failed to make temporary file for printing");
      return;
    }

    // now create a pipe and proceed to fork to write the postscript to temporary file
    PipeFifo fork_pipe(PipeFifo::block);

    // now fork to create the process which will write postscript to stdout
    pid_t pid = fork();

    if (pid == -1) {
      write_error("Fork error - exiting\n");
      std::exit(FORK_ERROR);
    }
  
    if (!pid) { // child process which will send postscript to stdout
  
      // this is the child process to write to stdin
      // now we have fork()ed we can connect to stderr
      connect_to_stderr();

      fork_pipe.connect_to_stdout();
      execvp(fax_to_ps_parms.first, fax_to_ps_parms.second);

      // if we reached this point, then the execvp() call must have failed
      // report error and then end process - use _exit(), not exit()
      write_error("Can't find the efix-0.9a program - please check your installation\n"
		  "and the PATH environmental variable\n");
      _exit(0);
    }

    // write from the pipe (containing the generated PS text) to file
    fork_pipe.make_readonly();
    write_pipe_to_file(&fork_pipe, file_fd);
    while (::close(file_fd) == -1 && errno == EINTR);

    print_manager_i->set_filename(tempfile_h.get());
    print_manager_i->print(); // this will pass ownership to the FilePrintManager object
                              // which will control its own destiny now, via the GTK+
                              // print system
  }
  else {
#endif

  std::pair<const char*, char* const*> print_from_stdin_parms(get_print_from_stdin_parms());
  if (print_from_stdin_parms.first) { // this will be 0 if get_print_from_stdin_parms()
                                      // threw a Utf8::ConversionError)

    // now launch a new process to control the printing process
    // the main program process needs to continue while the printing
    // is going on
    pid_t pid = fork();

    if (pid == -1) {
      write_error("Fork error - exiting\n");
      std::exit(FORK_ERROR);
    }
    if (!pid) { // child print process

      // make the call to connect_to_stderr() in this child process
      // dependent on calling a print confirmation popup dialog: if we have
      // elected not to bring up such a dialog, we are probably using a
      // print manager such as kprinter, and we can get confusing messages
      // from stderr from these
      // no mutex is required for the access to prog_config.print_popup
      // because we have fork()ed into a separate process
      if (prog_config.print_popup) connect_to_stderr();

      // now create a blocking pipe which the print processes can use to
      // communicate
      PipeFifo fork_pipe;
      try {
	fork_pipe.open(PipeFifo::block);
      }
      catch (PipeError&) {
	write_error("Cannot open pipe in FaxListDialog::print_fax_thread()\n");
	_exit(PIPE_ERROR);
      }
      
      pid_t pid = fork();

      if (pid == -1) {
	write_error("Fork error - exiting\n");
	_exit(FORK_ERROR);
      }
      if (!pid) { // process to exec to print_from_stdin()
      
	fork_pipe.connect_to_stdin();
	execvp(print_from_stdin_parms.first, print_from_stdin_parms.second);
	// if we reached this point, then the execvp() call must have failed
	// write error and then end process - use _exit(), not exit()
	write_error("Can't find the print program - please check your installation\n"
		    "and the PATH environmental variable\n");
	_exit(0);
      }

      // this is the process which will send postscript to stdout
      fork_pipe.connect_to_stdout();
      execvp(fax_to_ps_parms.first, fax_to_ps_parms.second);

      // if we reached this point, then the execvp() call must have failed
      // report error and then end process - use _exit(), not exit()
      write_error("Can't find the efix-0.9a program - please check your installation\n"
		  "and the PATH environmental variable\n");
      _exit(0);
    }
    // release the memory allocated on the heap for
    // the redundant print_from_stdin_parms and fax_to_ps_parms
    // we are in the main parent process here - no worries about
    // only being able to use async-signal-safe functions
    delete_parms(print_from_stdin_parms);
  }
#if GTK_CHECK_VERSION(2,10,0)
  }
#endif
  delete_parms(fax_to_ps_parms);
}


void FaxListDialog::view_fax(void) {
 
  bool have_view_cmd;
  // lock the Prog_config object to stop it being modified or accessed in the initial (GUI)
  // thread while we are accessing it here
  {
    Thread::Mutex::Lock lock(*prog_config.mutex_p);
    have_view_cmd = !prog_config.ps_view_cmd.empty();
  }

  if (fax_list_manager.is_fax_selected() == 1 && have_view_cmd) {

    std::auto_ptr<std::string> fax_basename_a(new std::string(prog_config.working_dir));
    if (mode == FaxListEnum::received) *fax_basename_a += "/faxin/";
    else *fax_basename_a += "/faxsent/";

    // we don't need to use a Glib conversion function here - we know the
    // fax name is just plain ASCII numbers
    GcharSharedHandle fax_number_h(fax_list_manager.get_fax_number());
    std::string fax_number;
    // as we checked for a fax selection, fax_number_h should not be
    // null, but check it in case
    if (fax_number_h.get()) fax_number = fax_number_h.get();
    *fax_basename_a += fax_number;
    *fax_basename_a += '/';
    *fax_basename_a += fax_number;
    *fax_basename_a += '.';

    // get a temporary file
    std::auto_ptr<std::string> filename_a(new std::string(prog_config.working_dir));
    *filename_a += "/efax-gtk-view-XXXXXX";

    std::string::size_type size = filename_a->size() + 1;
    SharedHandle<char*> tempfile_h(new char[size]);
    std::memcpy(tempfile_h.get(), filename_a->c_str(), size); // this will include the terminating '\0' in the copy
    int file_fd = mkstemp(tempfile_h.get());

    if (file_fd == -1) {
      write_error("Failed to make temporary file for viewing");
      return;
    }

    view_files.push_back(tempfile_h);
    *filename_a = tempfile_h.get();

    // now block off the signals for which we have set handlers so that the socket server
    // thread does not receive the signals, otherwise we will have memory synchronisation
    // issues in multi-processor systems - we will unblock in the initial (GUI) thread
    // as soon as the socket server thread has been launched
    sigset_t sig_mask;
    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, SIGCHLD);
    sigaddset(&sig_mask, SIGQUIT);
    sigaddset(&sig_mask, SIGTERM);
    sigaddset(&sig_mask, SIGINT);
    sigaddset(&sig_mask, SIGHUP);
    pthread_sigmask(SIG_BLOCK, &sig_mask, 0);

    // hand ownership of the file_pair arguments to the new thread - Thread::Thread::start()
    // could throw in the event of memory allocation failure, as could the creation of
    // the Callback2 object on free store, but as a matter of policy we do not deal with
    // that condition in this program so we can pass a raw pointer rather than the
    // std::auto_ptr<> object
    std::string* fax_basename_p = fax_basename_a.release();
    std::pair<std::string*, int> file_pair;
    file_pair.first = filename_a.release();
    file_pair.second = file_fd;
    std::auto_ptr<Thread::Thread> res =
      Thread::Thread::start(Callback::make(*this, &FaxListDialog::view_fax_thread,
					   fax_basename_p, file_pair),
			    false);
    if (!res.get()) {
      delete fax_basename_p;
      delete file_pair.first;
      while (::close(file_fd) == -1 && errno == EINTR);
      write_error("Cannot start new view fax thread, fax cannot be viewed\n");
    }
    // now unblock the signals so that the initial (GUI) thread can receive them
    pthread_sigmask(SIG_UNBLOCK, &sig_mask, 0);
  }
}

void FaxListDialog::view_fax_thread(std::string* fax_basename_p,
				    std::pair<std::string*, int> file_pair) {
 
  const std::auto_ptr<std::string> fax_basename_a(fax_basename_p);
  const std::auto_ptr<std::string> filename_a(file_pair.first);

  // now create a pipe and proceed to fork to write the postscript to temporary file
  PipeFifo fork_pipe(PipeFifo::block);

  // get the arguments for the exec() call to convert the fax files to
  // PS format following the next fork() call (because this is a multi-
  // threaded program, we must do this before fork()ing because we use
  // functions to get the arguments which are not async-signal-safe)
  std::pair<const char*, char* const*> fax_to_ps_parms(get_fax_to_ps_parms(*fax_basename_a, false));

  // now fork to create the process which will write postscript to stdout
  pid_t pid = fork();

  if (pid == -1) {
    write_error("Fork error - exiting\n");
    std::exit(FORK_ERROR);
  }
    
  if (!pid) { // child process which will send postscript to stdout
  
    // this is the child process to write to stdin
    // now we have fork()ed we can connect to stderr
    connect_to_stderr();

    fork_pipe.connect_to_stdout();
    execvp(fax_to_ps_parms.first, fax_to_ps_parms.second);

    // if we reached this point, then the execvp() call must have failed
    // report error and then end process - use _exit(), not exit()
    write_error("Can't find the efix-0.9a program - please check your installation\n"
		"and the PATH environmental variable\n");
    _exit(0);
  }
  // this is the parent process,

  // first, release the memory allocated on the heap for
  // the redundant fax_to_ps_parms
  // we are in the main parent process here - no worries about
  // only being able to use async-signal-safe functions
  delete_parms(fax_to_ps_parms);
    
  // now write from the pipe (containing the generated PS text) to file
  fork_pipe.make_readonly();
  write_pipe_to_file(&fork_pipe, file_pair.second);
  while (::close(file_pair.second) == -1 && errno == EINTR);

  // the temporary file to be viewed has now been created
  // so launch a new process to control the viewing process
  // we will reset handlers and then fork() again having done
  // so, so that we can wait on the child of child

  // first get the arguments for the exec() call to launch the fax
  // viewer following the next fork() call (because this is a multi-
  // threaded program, we must do this before fork()ing because we use
  // functions to get the arguments which are not async-signal-safe)
  std::pair<const char*, char* const*> ps_viewer_parms(get_ps_viewer_parms(filename_a->c_str()));

  if (ps_viewer_parms.first) { // this will be 0 if get_ps_viewer_parms()
                               // threw a Utf8::ConversionError)
    
    pid = fork();
    
    if (pid == -1) {
      write_error("Fork error\n");
      std::exit(FORK_ERROR);
    }
    if (!pid) { // child view-control process

      // unblock signals as these are blocked for all worker threads
      // (the child process inherits the signal mask of the thread
      // creating it with the fork() call)
      sigset_t sig_mask;
      sigemptyset(&sig_mask);
      sigaddset(&sig_mask, SIGCHLD);
      sigaddset(&sig_mask, SIGQUIT);
      sigaddset(&sig_mask, SIGTERM);
      sigaddset(&sig_mask, SIGINT);
      sigaddset(&sig_mask, SIGHUP);
      // this child process is single threaded, so we can use sigprocmask()
      // rather than pthread_sigmask() (and should do so as sigprocmask()
      // is guaranteed to be async-signal-safe)
      // this process will not be receiving interrupts so we do not need
      // to test for EINTR on the call to sigprocmask()
      sigprocmask(SIG_UNBLOCK, &sig_mask, 0);

      connect_to_stderr();

      execvp(ps_viewer_parms.first, ps_viewer_parms.second);

      // if we reached this point, then the execvp() call must have failed
      // report error and then end process - use _exit(), not exit()
      write_error("Can't find the ps viewer program - please check your installation\n"
		  "and the PATH environmental variable\n");
      _exit(0);
    }
    // release the memory allocated on the heap for
    // the redundant ps_viewer_parms
    // we are in the main parent process here - no worries about
    // only being able to use async-signal-safe functions
    delete_parms(ps_viewer_parms);
  }
}

void FaxListDialog::display_new_fax_count(void) {

  // if mode == FaxListEnum::sent we don't do anything at the moment
  if (mode == FaxListEnum::received) {
    int fax_count = get_new_fax_count_sig();
    
    // Note: if fax_count == 0 we could pass NULL as the colour argument to
    // gtk_widget_modify_fg() to bring up the default colour, but unfortunately
    // this does not work with GTK+ 2.0, so we will explicitly allocate a black
    // colour where fax_count == 0
    GdkColor fg_colour;
    if (fax_count) fg_colour.red = static_cast<guint16>(0.9 * 65535.0); // red colour
    else fg_colour.red = 0;                                             // black colour

    fg_colour.green = 0;
    fg_colour.blue = 0;
    // we don't need to allocate the GdkColor fg_colour object before it is
    // used by gtk_widget_modify_fg() - that function will do it itself
    gtk_widget_modify_fg(new_fax_label_p, GTK_STATE_NORMAL, &fg_colour);

#ifdef HAVE_STRINGSTREAM
    std::ostringstream strm;
    strm << gettext("New faxes:");
    strm << std::setfill(' ') << std::setw(4) << fax_count;
    gtk_label_set_text(GTK_LABEL(new_fax_label_p), strm.str().c_str());
#else
    std::ostrstream strm;
    strm << gettext("New faxes: ");
    strm << std::setfill(' ') << std::setw(4) << fax_count << std::ends;
    const char* text = strm.str();
    gtk_label_set_text(GTK_LABEL(new_fax_label_p), text);
    delete[] text;
#endif
  }
}

void FaxListDialog::delete_parms(std::pair<const char*, char* const*> parms_pair) {

  delete[] parms_pair.first;
  char* const* temp_pp = parms_pair.second;
  for(; *temp_pp != 0; ++temp_pp) {
    delete[] *temp_pp;
  }
  delete[] parms_pair.second;
}

namespace { // callbacks for internal use only

void EntryDialogCB::entry_dialog_selected(GtkWidget* widget_p, void* data) {
  EntryDialog* instance_p = static_cast<EntryDialog*>(data);

  if (widget_p == instance_p->ok_button_p) {
    if (instance_p->selected_impl()) instance_p->close();
  }
  else if (widget_p == instance_p->cancel_button_p) instance_p->close();
  else {
    write_error("Callback error in EntryDialogCB::entry_dialog_selected()\n");
    instance_p->close();
  }
}


gboolean EntryDialogCB::entry_dialog_key_press_event(GtkWidget*, GdkEventKey* event_p, void* data) {
  EntryDialog* instance_p = static_cast<EntryDialog*>(data);
  int keycode = event_p->keyval;
  bool cancel_focus = GTK_WIDGET_HAS_FOCUS(instance_p->cancel_button_p);

  if (keycode == GDK_Return && !cancel_focus) {
    if (instance_p->selected_impl()) instance_p->close();
    return true;  // stop processing here
  }
  return false;   // continue processing key events
}

} // anonymous namespace

EntryDialog::EntryDialog(const int standard_size, const char* entry_text,
			 const char* caption, const char* label_text,
			 GtkWindow* parent_p):
                             WinBase(caption, prog_config.window_icon_h,
				     true, parent_p) {


  ok_button_p = gtk_button_new_from_stock(GTK_STOCK_OK);
  cancel_button_p = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
  entry_p = gtk_entry_new();
  GtkWidget* button_box_p = gtk_hbutton_box_new();
  GtkWidget* label_p = gtk_label_new(label_text);
  GtkTable* table_p = GTK_TABLE(gtk_table_new(3, 1, false));

  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box_p), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing(GTK_BUTTON_BOX(button_box_p), standard_size/2);

  gtk_container_add(GTK_CONTAINER(button_box_p), cancel_button_p);
  gtk_container_add(GTK_CONTAINER(button_box_p), ok_button_p);

  gtk_table_attach(table_p, label_p,
		   0, 1, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL | GTK_EXPAND),
		   standard_size/2, standard_size/4);

  gtk_table_attach(table_p, entry_p,
		   0, 1, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_EXPAND,
		   standard_size/2, standard_size/4);

  gtk_table_attach(table_p, button_box_p,
		   0, 1, 2, 3,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/2, standard_size/4);

  g_signal_connect(G_OBJECT(ok_button_p), "clicked",
		   G_CALLBACK(EntryDialogCB::entry_dialog_selected), this);
  g_signal_connect(G_OBJECT(cancel_button_p), "clicked",
		   G_CALLBACK(EntryDialogCB::entry_dialog_selected), this);
  g_signal_connect(G_OBJECT(get_win()), "key_press_event",
		   G_CALLBACK(EntryDialogCB::entry_dialog_key_press_event), this);


  GTK_WIDGET_SET_FLAGS(ok_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS(cancel_button_p, GTK_CAN_DEFAULT);

  gtk_container_add(GTK_CONTAINER(get_win()), GTK_WIDGET(table_p));
  
  gtk_entry_set_text(GTK_ENTRY(entry_p), entry_text);
  gtk_widget_set_size_request(entry_p, standard_size * 9, standard_size);
  gtk_widget_grab_focus(entry_p);

  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_container_set_border_width(GTK_CONTAINER(get_win()), standard_size/2);

  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_resizable(get_win(), false);

  gtk_widget_show_all(GTK_WIDGET(get_win()));

  // we now need to deselect what is in the entry
  gtk_editable_select_region(GTK_EDITABLE(entry_p), 0, 0);
}

bool EntryDialog::selected_impl(void) {

  bool return_val = false;

  const gchar* text = gtk_entry_get_text(GTK_ENTRY(entry_p));
  
  if (!*text) beep();
  else {
    accepted(text);
    return_val = true;
  }
  return return_val;
}

DescriptionDialog::DescriptionDialog(const int standard_size,
				     const char* text,
				     GtkWindow* parent_p):
                      EntryDialog(standard_size,
				  text,
				  gettext("efax-gtk: Fax description"),
				  gettext("Fax description?"),
				  parent_p) {}

AddFolderDialog::AddFolderDialog(const int standard_size,
				 GtkWindow* parent_p):
                      EntryDialog(standard_size,
				  "",
				  gettext("efax-gtk: Add folder"),
				  gettext("Folder name?\n"
					  "(Note this will be placed in the top\n"
					  "level and can be drag-and-dropped\n"
					  "into other folders)"),
				  parent_p) {}
