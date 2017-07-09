/* Copyright (C) 2001 to 2005 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#include <gtk/gtkbutton.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkbbox.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktextview.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtklabel.h>

#include <pango/pango-font.h>
#include <gdk/gdkpixbuf.h>
#include <gdk/gdktypes.h>
#include <gdk/gdkkeysyms.h> // the key codes are here

#include "helpfile.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#endif

int HelpDialog::is_helpfile = 0;

namespace { // callbacks for internal use only

void HelpDialogCB::help_button_clicked(GtkWidget*, void* data) {
  static_cast<HelpDialog*>(data)->close();
}

gboolean HelpDialogCB::help_key_press_event(GtkWidget*, GdkEventKey* event_p, void* data) {
  HelpDialog* instance_p = static_cast<HelpDialog*>(data);
  int keycode = event_p->keyval;

  if (keycode == GDK_Home || keycode == GDK_End
      || keycode == GDK_Up || keycode == GDK_Down
      || keycode == GDK_Page_Up || keycode == GDK_Page_Down) {
    gtk_widget_event(gtk_notebook_get_nth_page(instance_p->notebook_p,
					       gtk_notebook_get_current_page(instance_p->notebook_p)),
		     (GdkEvent*)event_p);
    return true;    // stop processing here
  }
  return false;     // pass on the key event
}

}  // anonymous namespace

HelpDialog::HelpDialog(const int standard_size): 
                             WinBase(gettext("efax-gtk: Help"),
				     prog_config.window_icon_h) {

  // notify the existence of this object
  is_helpfile++;

  GtkWidget* close_button_p = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
  
  GtkWidget* button_box_p = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box_p), GTK_BUTTONBOX_SPREAD);
  gtk_container_add(GTK_CONTAINER(button_box_p), close_button_p);

  notebook_p = GTK_NOTEBOOK(gtk_notebook_new());
  GtkWidget* scrolled_window_p;
  GtkWidget* label_p;

  // make and append sending help page
  scrolled_window_p = make_scrolled_window();
  gtk_container_add(GTK_CONTAINER(scrolled_window_p),
		    make_text_view(get_sending_help()));
  label_p = gtk_label_new(gettext("Sending"));
  gtk_notebook_append_page(notebook_p, scrolled_window_p, label_p);

  // make and append receiving help page
  scrolled_window_p = make_scrolled_window();
  gtk_container_add(GTK_CONTAINER(scrolled_window_p),
		    make_text_view(get_receiving_help()));
  label_p = gtk_label_new(gettext("Receiving"));
  gtk_notebook_append_page(notebook_p, scrolled_window_p, label_p);

  // make and append addressbook help page
  scrolled_window_p = make_scrolled_window();
  gtk_container_add(GTK_CONTAINER(scrolled_window_p),
		    make_text_view(get_addressbook_help()));
  label_p = gtk_label_new(gettext("Address Book"));
  gtk_notebook_append_page(notebook_p, scrolled_window_p, label_p);

  // make and append fax list help page
  scrolled_window_p = make_scrolled_window();
  gtk_container_add(GTK_CONTAINER(scrolled_window_p),
		    make_text_view(get_fax_list_help()));
  label_p = gtk_label_new(gettext("Fax Lists"));
  gtk_notebook_append_page(notebook_p, scrolled_window_p, label_p);

  // make and append settings help page
  scrolled_window_p = make_scrolled_window();
  gtk_container_add(GTK_CONTAINER(scrolled_window_p),
		    make_text_view(get_settings_help()));
  label_p = gtk_label_new(gettext("Settings"));
  gtk_notebook_append_page(notebook_p, scrolled_window_p, label_p);

  gtk_notebook_set_tab_pos(notebook_p, GTK_POS_TOP);
  gtk_notebook_set_scrollable(notebook_p, true);

  GtkBox* vbox_p = GTK_BOX(gtk_vbox_new(false, 0));
  gtk_box_pack_start(vbox_p, GTK_WIDGET(notebook_p), true, true, standard_size/3);
  gtk_box_pack_start(vbox_p, button_box_p, false, false, standard_size/3);

  g_signal_connect(G_OBJECT(close_button_p), "clicked",
		   G_CALLBACK(HelpDialogCB::help_button_clicked), this);
  g_signal_connect(G_OBJECT(get_win()), "key_press_event",
		   G_CALLBACK(HelpDialogCB::help_key_press_event), this);

  gtk_container_add(GTK_CONTAINER(get_win()), GTK_WIDGET(vbox_p));

  gtk_window_set_default_size(get_win(), standard_size * 25, standard_size * 14);
  
  gtk_container_set_border_width(GTK_CONTAINER(get_win()), standard_size/4);
  //gtk_window_set_position(get_win(), GTK_WIN_POS_NONE);
  
  gtk_widget_grab_focus(GTK_WIDGET(get_win()));

  GTK_WIDGET_SET_FLAGS(close_button_p, GTK_CAN_DEFAULT);

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

HelpDialog::~HelpDialog(void) {
  // notify the destruction of this object
  is_helpfile--;
}

GtkWidget* HelpDialog::make_text_view(const char* text) {

  GtkTextView* text_view_p = GTK_TEXT_VIEW(gtk_text_view_new());
  gtk_text_view_set_wrap_mode(text_view_p, GTK_WRAP_WORD);
  gtk_text_view_set_editable(text_view_p, false);
  PangoFontDescription* font_description = 
    pango_font_description_from_string(prog_config.fixed_font.c_str());
  gtk_widget_modify_font(GTK_WIDGET(text_view_p), font_description);
  pango_font_description_free(font_description);
  gtk_text_buffer_set_text(gtk_text_view_get_buffer(text_view_p),
			   text,
			   -1);
  GTK_WIDGET_UNSET_FLAGS(GTK_WIDGET(text_view_p), GTK_CAN_FOCUS);
  return GTK_WIDGET(text_view_p);
}

GtkWidget* HelpDialog::make_scrolled_window(void) {

  GtkScrolledWindow* scrolled_window_p = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(0, 0));
  gtk_scrolled_window_set_shadow_type(scrolled_window_p, GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy(scrolled_window_p, GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  return GTK_WIDGET(scrolled_window_p);
}

const char* HelpDialog::get_sending_help(void) {

  // Note for Translator: the sixth paragraph will require re-translation
  // if the most recent translation is dated before 6th November 2004
  return gettext("\n"
		 "Sending faxes\n"
		 "-------------\n"
		 "\n"
		 "Before sending a fax, it must be specified in the \"Fax to send\" box. "
		 "It must be in postscript format (a format produced by all Unix/Linux "
		 "word and document processors), and will be converted by the program "
		 "into the correct tiffg3 fax format.\n"
		 "\n"
		 "There are two fax entry methods.  First, the fax to be sent can be a "
		 "file saved on the filesystem.  It can be entered manually in the \"Fax "
		 "to send\" box, or entered by means of the file selection dialog.  If "
		 "the file comprises a single postscript file, then you can find it by "
		 "pressing the \"Single File\" button.  It can be more easily found with "
		 "this dialog if it is placed in the $HOME/faxout directory.\n"
		 "\n"
		 "Where more than one file is specified in the \"Fax to send\" box, they "
		 "will be sent as a single fax appended in the order in which they are "
		 "entered in the box.  Such multiple files can be more easily selected "
		 "using the file list brought up by pressing the \"Multiple Files\" "
		 "button.  Pressing the \"Multiple Files\" button enables files to be "
		 "found and added to the file list, and they can be reordered by using "
		 "the Up or Down arrow buttons, or by dragging and dropping with the "
		 "mouse.\n"
		 "\n"
		 "As an alternative, faxes can be received directly from the print "
		 "system by means of a socket server provided by the program.  Efax-gtk "
		 "maintains a list of queued faxes received from the socket which can be "
		 "accessed by choosing \"Socket\" as the fax entry method, and then "
		 "bringing up the queued faxes list by pressing the \"Socket list\" "
		 "button.  This is a more convenient way of sending faxes from a word "
		 "processor, and enables a fax to be sent for faxing to efax-gtk by "
		 "printing from the word processor program.  Where a fax is queued for "
		 "sending in the socket list, a small red circle will appear in the main "
		 "program window on the right hand side of the \"Fax to send\" box.  For "
		 "particulars of how to set up CUPS or lpr/lprng to send to efax-gtk, see "
		 "the README file which comes with the distribution.\n"
		 "\n"
		 "The telephone number to which the fax is to be sent is entered into "
		 "the \"Tel number\" box.  This can be entered directly into the box, or "
		 "by using the built-in addressbook.  The addressbook can be invoked by "
		 "pressing the \"Tel number\" button, or from the `File/Address book' "
		 "pull-down menu item (see the \"Address Book\" tag in this help dialog). "
		 "However, if a telephone connection has already been established with "
		 "the remote fax receiver, then the fax can be sent without dialing by "
		 "leaving the \"Tel number\" box blank (a dialog will come up asking if "
		 "you would like to send the fax without dialing).\n"
		 "\n"
		 "When a fax is received from the print system via the socket server, "
		 "the program settings can also be configured to bring up a dialog "
		 "automatically.  If the program is inactive or is standing-by to "
		 "receive faxes the fax can be sent directly from this dialog without "
		 "the need to invoke the list of queued faxes received from the socket.\n"
		 "\n"
		 "Successfully sent faxes are copied to a directory in the $HOME/faxsent "
		 "directory, which has a name derived from the year, month, day, hour "
		 "and seconds when the sending of the fax was completed, and will appear "
		 "in the faxes sent list.  They are only included in that list if they "
		 "have been sent without error.  The efax message display box will "
		 "report on the progress of a fax being sent.  The fax list can be "
		 "brought up from the `File/List sent faxes' pull down menu item.  See "
		 "\"Using the fax lists\" further below.\n");
}

const char* HelpDialog::get_receiving_help(void) {

  return gettext("\n"
		 "Receiving faxes\n"
		 "---------------\n"
		 "\n"
		 "Three ways of receiving faxes are provided for.\n"
		 "\n"
		 "First, the program can be set to answer a fax call which is ringing "
		 "but has not been answered, by pressing the \"Answer call\" button.\n"
		 "\n"
		 "Secondly, the program can take over a call which has already been "
		 "answered (say, by a telephone hand set) by pressing the \"Take over "
		 "call\" button.\n"
		 "\n"
		 "Thirdly, the program can be placed in standby mode by pressing the "
		 "\"Standby\" button.  This will automatically answer any call after the "
		 "number of rings specified in the efax-gtkrc file, and receive the fax. "
		 "The program will keep on receiving faxes until the \"Stop\" button is "
		 "pressed.\n"
		 "\n"
		 "Received faxes in tiffg3 format (one file for each page) are placed in "
		 "a directory in the $HOME/faxin directory, which has a name derived "
		 "from the year, month, day, hour and seconds when reception of the fax "
		 "was completed, and is the fax ID number.\n"
		 "\n"
		 "Received faxes can be printed, viewed, described and managed using the "
		 "built in fax list facility.  This can be brought up from the "
		 "`File/List received faxes' pull down menu item.  See \"Using the fax "
		 "lists\" further below.\n"
		 "\n"
		 "When a fax is received, a pop-up dialog can also be set to appear (go "
		 "to the Settings dialog to do this).  In the settings dialog you can "
		 "also specify a program to be executed when a fax is received.  The "
		 "fax ID number is passed as the first (and only) argument to the "
		 "program, which enables the program to find the fax in $HOME/faxin.  "
		 "The distribution contains two executable scripts, mail_fax and "
		 "print_fax, which can be used to e-mail a fax or print a fax to a user "
		 "automatically when it is received.  (These scripts are not installed "
		 "by 'make install' - if you want to use them, make them executable with "
		 "'chmod +x' and copy them to a directory which is in the system path "
		 "such as /usr/local/bin, and then specify the script name in the "
		 "settings dialog.)\n");
}

const char* HelpDialog::get_addressbook_help(void) {

  return gettext("\n"
		 "Using the address book\n"
		 "----------------------\n"
		 "\n"
		 "To pick a telephone number from the address book, highlight the "
		 "relevant address by pressing the left mouse button over it, and then "
		 "press the \"OK\" button.\n"
		 "\n"
		 "Addresses can be added to the address book by pressing the add button, "
		 "and then completing the relevant dialog which will appear.  To delete "
		 "an address from the address book, highlight the relevant address and "
		 "press the delete (trashcan) button.  The addressbook can be sorted by "
		 "using the up and down arrow buttons on a highlighted address, or by "
		 "dragging and dropping using the mouse.\n");
}

const char* HelpDialog::get_fax_list_help(void) {

  return gettext("\n"
		 "Using the fax lists\n"
		 "-------------------\n"
		 "\n"
		 "To bring up the fax lists, go to the the `File' menu and pick the "
		 "`List received faxes' or `List sent faxes' menu item.  Highlight the "
		 "fax to printed or viewed by pressing the left mouse button.  The "
		 "programs to be used to print and view the fax are specifed in the "
		 "efax-gtkrc configuration file, or if none are specified, the program "
		 "will print using lpr (which will work for most Unix systems) and view "
		 "with gv.\n"
		 "\n"
		 "To print faxes, a PRINT_SHRINK parameter can be specifed in efax-gtkrc "
		 "to enable the fax page to fit within the printer margins.  A parameter "
		 "of 98 will work with most printers.  This can be changed while the "
		 "program is running by bringing up the `Settings' dialog and entering "
		 "it into the `Print/Print Shrink' box.\n"
		 "\n"
		 "A description can be added to a received fax when appearing in a fax "
		 "list (or subsequently amended) by pressing the relevant button -- this "
		 "will enable faxes to be more easily identified.\n"
		 "\n"
		 "The received faxes list will show, at the far right of the tool bar, "
		 "the number of faxes received since the program was last started.  If "
		 "efax-gtk is in receive standby mode, the `tooltips' for the program's "
		 "icon in the system tray will also indicate this number.  The count can "
		 "be reset to 0 without restarting the program by pressing the reset "
		 "button in the received faxes list.\n");
}

const char* HelpDialog::get_settings_help(void) {

  return gettext("\n"
		 "Settings\n"
		 "--------\n"
		 "\n"
		 "The program settings can be changed by manually editing the efax-gtk "
		 "configuration file comprising $HOME/.efax-gtkrc, $sysconfdir/efax-gtkrc "
		 "or /etc/efax-gtkrc.  The file is searched for in that order, so "
		 "$HOME/.efax-gtkrc takes precedence over the other two.\n"
		 "\n"
		 "The configuration file can also be set by using the Settings dialog "
		 "launched from the `File/Settings' pull down menu item.  The settings "
		 "entered using this dialog are always stored as $HOME/.efax-gtkrc. "
		 "Accordingly, if the Settings dialog has been used, and you want to "
		 "revert to the global settings, this can be done either by deleting the "
		 "$HOME/.efax-gtkrc file, or by pressing the `Reset' button in the "
		 "Settings dialog, which will reload the Settings dialog from the "
		 "global configuration file ($sysconfdir/efax-gtkrc or /etc/efax-gtkrc).\n"
		 "\n"
		 "Help can be obtained when filling out the Settings dialog by holding "
		 "the mouse over the relevant help (?) button, which will bring up a "
		 "\"Tips\" display, or by pressing the button, which will bring up an "
		 "information display.\n");
}
