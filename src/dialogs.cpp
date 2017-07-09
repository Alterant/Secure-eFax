/* Copyright (C) 2001 to 2006 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkbbox.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkversion.h>
#include <gtk/gtkstock.h>

#include <gdk/gdkpixbuf.h>
#include <gdk/gdkkeysyms.h> // the key codes are here

#if GTK_CHECK_VERSION(2,4,0)
#  undef EFAX_GTK_USE_FILE_SEL
#else
#  define EFAX_GTK_USE_FILE_SEL 1
#endif

#ifdef EFAX_GTK_USE_FILE_SEL
#  include <gtk/gtkfilesel.h>
#  define EFAX_GTK_FILE_DIALOG_NEW gtk_file_selection_new(0)
#else
#  include <gtk/gtkfilechooserdialog.h>
#  include <glib/glist.h>
#  define EFAX_GTK_FILE_DIALOG_NEW (gtk_file_chooser_dialog_new(0, 0, \
                                   GTK_FILE_CHOOSER_ACTION_OPEN, \
                                   gettext("View"), GTK_RESPONSE_NONE, \
                                   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, \
                                   GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, \
                                   static_cast<void*>(0)))
#endif

#include <pango/pango-font.h>
#include <pango/pango-layout.h>

#include "dialogs.h"
#include "utils/shared_handle.h"
#include "utils/gobj_handle.h"
#include "utils/mutex.h"
#include "utils/utf8_utils.h"
#include "gpl.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#endif


namespace { // callbacks for internal use only

void FileReadSelectDialogCB::file_selected(GtkWidget* widget_p, void* data) {
  FileReadSelectDialog* instance_p = static_cast<FileReadSelectDialog*>(data);
  gtk_widget_hide_all(GTK_WIDGET(instance_p->get_win()));
  if (widget_p == instance_p->ok_button_p) instance_p->set_result();
  else if (widget_p == instance_p->cancel_button_p) instance_p->result.clear();
  else {
    instance_p->result.clear();
    write_error("Callback error in FileReadSelectDialogCB::file_selected()\n");
  }
  instance_p->close();
}

void FileReadSelectDialogCB::file_view_file(GtkWidget*, void* data) {
  static_cast<FileReadSelectDialog*>(data)->view_file_impl();
}

} // anonymous namespace

FileReadSelectDialog::FileReadSelectDialog(int size, bool multi_files, GtkWindow* parent_p):
                                WinBase(gettext("efax-gtk: File to fax"),
					prog_config.window_icon_h,
					true, parent_p,
					GTK_WINDOW(EFAX_GTK_FILE_DIALOG_NEW)),
				standard_size(size) {

  GtkWidget* view_button_p;

#ifdef EFAX_GTK_USE_FILE_SEL
  GtkFileSelection* file_object_p = GTK_FILE_SELECTION(get_win());

  gtk_file_selection_set_select_multiple(file_object_p, multi_files);
  if (!prog_config.working_dir.empty()) {
    std::string temp(prog_config.working_dir);
    temp +=  "/faxout/";
    gtk_file_selection_set_filename(file_object_p, temp.c_str());
  }

  gtk_file_selection_hide_fileop_buttons(file_object_p);

  GtkBox* hbox_p = GTK_BOX(file_object_p->action_area);
  gtk_box_set_homogeneous(hbox_p, false);

  GtkWidget* label_p = gtk_label_new(0);
  view_button_p = gtk_button_new_with_label(gettext("View"));
  gtk_box_pack_start(hbox_p, label_p, true, false, 0);
  gtk_box_pack_start(hbox_p, view_button_p, false, false, 0);

  GTK_WIDGET_SET_FLAGS(view_button_p, GTK_CAN_DEFAULT);

  ok_button_p = file_object_p->ok_button;
  cancel_button_p = file_object_p->cancel_button;

#else
  GtkFileChooser* file_object_p = GTK_FILE_CHOOSER(get_win());

  gtk_file_chooser_set_select_multiple(file_object_p, multi_files);
  if (!prog_config.working_dir.empty()) {
    std::string temp(prog_config.working_dir);
    temp +=  "/faxout";
    gtk_file_chooser_set_current_folder(file_object_p, temp.c_str());
  }

  GtkBox* hbox_p = GTK_BOX(GTK_DIALOG(get_win())->action_area);
  gtk_box_set_homogeneous(hbox_p, true);

  GList* button_list = gtk_container_get_children(GTK_CONTAINER(hbox_p));

  if (g_list_length(button_list) < 3) {
    write_error("Button creation error in FileReadSelectDialog::FileReadSelectDialog()\n");
    gtk_widget_show_all(GTK_WIDGET(get_win()));
    g_list_free(button_list);
    return;
  }
  ok_button_p = static_cast<GtkWidget*>(g_list_nth_data(button_list, 0));
  cancel_button_p =  static_cast<GtkWidget*>(g_list_nth_data(button_list, 1));;
  view_button_p =  static_cast<GtkWidget*>(g_list_nth_data(button_list, 2));;

  g_list_free(button_list);

#endif

  g_signal_connect(G_OBJECT(ok_button_p), "clicked",
		   G_CALLBACK(FileReadSelectDialogCB::file_selected), this);
  g_signal_connect(G_OBJECT(cancel_button_p), "clicked",
		   G_CALLBACK(FileReadSelectDialogCB::file_selected), this);
  g_signal_connect(G_OBJECT(view_button_p), "clicked",
		   G_CALLBACK(FileReadSelectDialogCB::file_view_file), this);

  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);
  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);

  gtk_widget_show_all(GTK_WIDGET(get_win()));

#ifdef EFAX_GTK_USE_FILE_SEL
  // now get and set the button size for view_button
  gtk_widget_set_size_request(view_button_p,
			      cancel_button_p->allocation.width,
			      cancel_button_p->allocation.height);
#endif
}

void FileReadSelectDialog::on_delete_event(void) {
  gtk_widget_hide_all(GTK_WIDGET(get_win()));
  result.clear();
  close();
}

std::pair<const char*, char* const*> FileReadSelectDialog::get_view_file_parms(void) {

  std::vector<std::string> view_parms;
  std::string view_cmd;
  std::string view_name;
  std::string::size_type end_pos;
  try {
    // lock the Prog_config object to stop it being accessed in
    // FaxListDialog::get_ps_viewer_parms() while we are accessing it here
    // (this is ultra cautious as it is only copied/checked for emptiness
    // there, and the GUI interface is insensitive if we are here)
    Thread::Mutex::Lock lock(*prog_config.mutex_p);
    view_cmd = Utf8::filename_from_utf8(prog_config.ps_view_cmd);
  }
  catch (Utf8::ConversionError&) {
    write_error("UTF-8 conversion error in FileReadSelectDialog::get_view_file_parms()\n");
    return std::pair<const char*, char* const*>(0,0);
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

  else { // just a view command without parameters to be passed
    view_name = view_cmd;
    view_parms.push_back(view_name);
  }

  view_parms.push_back(get_filename_string());

  char** exec_parms = new char*[view_parms.size() + 1];

  char**  temp_pp = exec_parms;
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

void FileReadSelectDialog::view_file_impl(void) {

  // check pre-conditions
  std::string filename(get_filename_string());
  if (filename.empty()
      || filename[filename.size() - 1] == '/'
      || access(filename.c_str(), R_OK)) {
    beep();
    return;
  }

  // get the arguments for the exec() call below (because this is a
  // multi-threaded program, we must do this before fork()ing because
  // we use functions to get the arguments which are not async-signal-safe)
  std::pair<const char*, char* const*> view_file_parms(get_view_file_parms());

  if (view_file_parms.first) { // this will be 0 if get_view_file_parms()
                               // threw a Utf8::ConversionError)

    pid_t pid = fork();

    if (pid == -1) {
      write_error("Fork error - exiting\n");
      std::exit(FORK_ERROR);
    }
    if (!pid) {  // child process - as soon as everything is set up we are going to do an exec()

      connect_to_stderr();

      execvp(view_file_parms.first, view_file_parms.second);

      // if we reached this point, then the execvp() call must have failed
      // report error and end process - use _exit() and not exit()
      write_error("Can't find the postscript viewer program - please check your installation\n"
		  "and the PATH environmental variable\n");
      _exit(0);
    } // end of view program process

    // release the memory allocated on the heap for
    // the redundant view_file_parms
    // we are in the main parent process here - no worries about
    // only being able to use async-signal-safe functions
    delete_parms(view_file_parms);
  }
}

void FileReadSelectDialog::delete_parms(std::pair<const char*, char* const*> parms_pair) {

  delete[] parms_pair.first;
  char* const* temp_pp = parms_pair.second;
  for(; *temp_pp != 0; ++temp_pp) {
    delete[] *temp_pp;
  }
  delete[] parms_pair.second;
}

std::string FileReadSelectDialog::get_filename_string(void) {
#ifdef EFAX_GTK_USE_FILE_SEL
  return std::string((const char*)gtk_file_selection_get_filename(GTK_FILE_SELECTION(get_win())));
#else
  GcharScopedHandle file_name_h(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(get_win())));
  if (file_name_h.get()) return std::string((const char*)file_name_h.get());
  return std::string();
#endif
}

void FileReadSelectDialog::set_result(void) {

  result.clear();
#ifdef EFAX_GTK_USE_FILE_SEL
  gchar** file_array = gtk_file_selection_get_selections(GTK_FILE_SELECTION(get_win()));
  gchar** temp_p = file_array;
  for (; *temp_p; temp_p++) {
    result.push_back((const char*)*temp_p);
  }
  g_strfreev (file_array);
#else
  GSList* file_list_p = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(get_win()));
  void* elem = g_slist_nth_data(file_list_p, 0); // first element
  for (guint count = 1; elem; count++) {
    result.push_back((const char*)elem);
    g_free(elem);
    elem = g_slist_nth_data(file_list_p, count);
  }
  g_slist_free(file_list_p);
#endif
  try {
    std::transform(result.begin(), result.end(), result.begin(),
		   Utf8::filename_to_utf8);
  }
  catch (Utf8::ConversionError&) {
    write_error("UTF-8 conversion error in FileReadSelectDialog::set_result()\n");
  }
}


namespace { // callbacks for internal use only

void GplDialogCB::gpl_selected(GtkWidget* widget_p, void* data) {
  GplDialog* instance_p = static_cast<GplDialog*>(data);
  gtk_widget_hide_all(GTK_WIDGET(instance_p->get_win()));
  if (widget_p == instance_p->accept_button_p) instance_p->result = GplDialog::accepted;
  else if (widget_p == instance_p->reject_button_p) instance_p->result = GplDialog::rejected;
  else write_error("Callback error in GplDialogCB::gpl_selected()\n");
  instance_p->close();
}

gboolean GplDialogCB::gpl_key_press_event(GtkWidget*, GdkEventKey* event_p, void* data) {
  GplDialog* instance_p = static_cast<GplDialog*>(data);
  int keycode = event_p->keyval;
  
  if (keycode == GDK_Escape) instance_p->on_delete_event();
  else if (keycode == GDK_Home || keycode == GDK_End
	   || keycode == GDK_Up || keycode == GDK_Down
	   || keycode == GDK_Page_Up || keycode == GDK_Page_Down) {
    gtk_widget_event(GTK_WIDGET(instance_p->text_view_p), (GdkEvent*)event_p);
  }
  return true;    // stop processing here
}

void GplDialogCB::gpl_style_set(GtkWidget*, GtkStyle*, void* data) {

  GplDialog* instance_p = static_cast<GplDialog*>(data);

  //now set the buttons in GplDialog
  int width = 0;
  int height = 0;
  // create a button to work on - the GobjHandle object will claim ownership of the button
  GobjHandle<GtkWidget> button_h(gtk_button_new());

  GtkStyle* style_p = gtk_widget_get_style(GTK_WIDGET(instance_p->get_win()));
  gtk_widget_set_style(button_h, style_p);

  GobjHandle<PangoLayout> pango_layout_h(gtk_widget_create_pango_layout(button_h,
							 instance_p->max_text.c_str()));
  pango_layout_get_pixel_size(pango_layout_h, &width, &height);

  // add padding
  width += 18;
  height += 12;

  // have some sensible minimum width and height if a very small font chosen
  const int min_width = instance_p->standard_size * 4;
  const int min_height = 30;
  if (width < min_width) width = min_width;
  if (height < min_height) height = min_height;

  gtk_widget_set_size_request(instance_p->accept_button_p, width, height);
  gtk_widget_set_size_request(instance_p->reject_button_p, width, height);
}

} // anonymous namespace

GplDialog::GplDialog(int size): WinBase(gettext("efax-gtk: Conditions, Notices and Disclaimers"),
				        prog_config.window_icon_h,
					true),
				standard_size(size),
				result(rejected) {

  accept_button_p = gtk_button_new_with_label(gettext("Accept"));
  reject_button_p = gtk_button_new_with_label(gettext("Reject"));
  
  GtkWidget* label_p = gtk_label_new(gettext("Do you accept the Conditions, Notices "
					     "and Disclaimers shown above?"));

  GtkTable* table_p = GTK_TABLE(gtk_table_new(3, 2, false));

  text_view_p = GTK_TEXT_VIEW(gtk_text_view_new());
  gtk_text_view_set_editable(text_view_p, false);

  PangoFontDescription* font_description = 
    pango_font_description_from_string(prog_config.fixed_font.c_str());
  gtk_widget_modify_font(GTK_WIDGET(text_view_p), font_description);
  pango_font_description_free(font_description);

  gtk_text_buffer_set_text(gtk_text_view_get_buffer(text_view_p),
			   gpl_copyright_msg()->c_str(),
			   -1);

  GtkScrolledWindow* scrolled_window_p = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(0, 0));
  gtk_scrolled_window_set_shadow_type(scrolled_window_p, GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy(scrolled_window_p, GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scrolled_window_p), GTK_WIDGET(text_view_p));

  get_longest_button_text();

  gtk_table_attach(table_p, GTK_WIDGET(scrolled_window_p),
		   0, 2, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL | GTK_EXPAND),
		   0, 0);

  gtk_table_attach(table_p, label_p,
		   0, 2, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   0, standard_size/3);

  gtk_table_attach(table_p, accept_button_p,
		   0, 1, 2, 3,
		   GTK_SHRINK, GTK_SHRINK,
		   0, standard_size/3);

  gtk_table_attach(table_p, reject_button_p,
		   1, 2, 2, 3,
		   GTK_SHRINK, GTK_SHRINK,
		   0, standard_size/3);

  g_signal_connect(G_OBJECT(get_win()), "style_set",
		   G_CALLBACK(GplDialogCB::gpl_style_set), this);
  g_signal_connect(G_OBJECT(accept_button_p), "clicked",
		   G_CALLBACK(GplDialogCB::gpl_selected), this);
  g_signal_connect(G_OBJECT(reject_button_p), "clicked",
		   G_CALLBACK(GplDialogCB::gpl_selected), this);
  g_signal_connect(G_OBJECT(get_win()), "key_press_event",
		   G_CALLBACK(GplDialogCB::gpl_key_press_event), this);

  gtk_container_add(GTK_CONTAINER(get_win()), GTK_WIDGET(table_p));
  
  gtk_window_set_default_size(get_win(), standard_size * 25, standard_size * 16);
  
  gtk_container_set_border_width(GTK_CONTAINER(get_win()), standard_size/4);
  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);
  
  gtk_widget_grab_focus(GTK_WIDGET(get_win()));

  GTK_WIDGET_UNSET_FLAGS(GTK_WIDGET(text_view_p), GTK_CAN_FOCUS);
  GTK_WIDGET_UNSET_FLAGS(accept_button_p, GTK_CAN_FOCUS);
  GTK_WIDGET_UNSET_FLAGS(reject_button_p, GTK_CAN_FOCUS);

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

int GplDialog::get_exec_val(void) const {
  return result;
}

void GplDialog::on_delete_event(void) {
  gtk_widget_hide_all(GTK_WIDGET(get_win()));
  result = rejected;
  close();
}

void GplDialog::get_longest_button_text(void) {
  std::vector<std::string> text_vec;
  text_vec.push_back(gettext("Accept"));
  text_vec.push_back(gettext("Reject"));

  // create a button to work on - the GobjHandle object will claim ownership of the button
  GobjHandle<GtkWidget> button_h(gtk_button_new());

  std::vector<std::string>::const_iterator temp_iter;
  std::vector<std::string>::const_iterator max_width_iter;
  int width;
  int height;
  int max_width;
  for (temp_iter = text_vec.begin(), max_width_iter = text_vec.begin(),
	 width = 0, height = 0, max_width = 0;
       temp_iter != text_vec.end(); ++temp_iter) {

    GobjHandle<PangoLayout> pango_layout_h(gtk_widget_create_pango_layout(button_h,
							      temp_iter->c_str()));
    pango_layout_get_pixel_size(pango_layout_h, &width, &height);

    if (width > max_width) {
      max_width = width;
      max_width_iter = temp_iter;
    }
  }
  max_text = *max_width_iter;
}


namespace { // callbacks for internal use only

void InfoDialogCB::info_selected(GtkDialog*, int, void* data) {
  static_cast<InfoDialog*>(data)->close();
}

} // anonymous namespace

InfoDialog::InfoDialog(const char* text, const char* caption,
                       GtkMessageType message_type, GtkWindow* parent_p, bool modal):
                       WinBase(caption, prog_config.window_icon_h,
			       modal, parent_p,
			       GTK_WINDOW(gtk_message_dialog_new(parent_p, GtkDialogFlags(0),
					  message_type, GTK_BUTTONS_CLOSE, text))) {

  // make further specialisations for a message dialog object

  if (!GTK_CHECK_VERSION(2, 4, 0)) {
    gtk_dialog_set_has_separator(GTK_DIALOG(get_win()), false);
  }

  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);

  gtk_button_box_set_layout(GTK_BUTTON_BOX(GTK_DIALOG(get_win())->action_area),
			    GTK_BUTTONBOX_SPREAD);

  g_signal_connect(G_OBJECT(get_win()), "response",
		   G_CALLBACK(InfoDialogCB::info_selected), this);

  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_resizable(get_win(), false);

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}


namespace { // callbacks for internal use only

void PromptDialogCB::prompt_selected(GtkWidget* widget_p, void* data) {
  PromptDialog* instance_p = static_cast<PromptDialog*>(data);
  gtk_widget_hide_all(GTK_WIDGET(instance_p->get_win()));
  if (widget_p == instance_p->ok_button_p) {
    instance_p->result = true;
    instance_p->accepted();
  }
  else if (widget_p == instance_p->cancel_button_p) {
    instance_p->result = false;
    instance_p->rejected();
  }
  else write_error("Callback error in PromptDialogCB::prompt_selected()\n");
  instance_p->close();
}

} // anonymous namespace

PromptDialog::PromptDialog(const char* text, const char* caption,
			   int standard_size, GtkWindow* parent_p, bool modal):
                               WinBase(caption, prog_config.window_icon_h,
				       modal, parent_p),
                               result(false) {

  ok_button_p = gtk_button_new_from_stock(GTK_STOCK_OK);
  cancel_button_p = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
  GtkWidget* button_box_p = gtk_hbutton_box_new();
  GtkWidget* label_p = gtk_label_new(text);
  GtkTable* table_p = GTK_TABLE(gtk_table_new(2, 1, false));

  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box_p), GTK_BUTTONBOX_END);
  gtk_button_box_set_spacing(GTK_BUTTON_BOX(button_box_p), standard_size/2);

  gtk_label_set_line_wrap(GTK_LABEL(label_p), true);

  gtk_container_add(GTK_CONTAINER(button_box_p), cancel_button_p);
  gtk_container_add(GTK_CONTAINER(button_box_p), ok_button_p);

  gtk_table_attach(table_p, label_p,
		   0, 1, 0, 1,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GtkAttachOptions(GTK_FILL | GTK_EXPAND),
		   standard_size/2, standard_size/4);

  gtk_table_attach(table_p, button_box_p,
		   0, 1, 1, 2,
		   GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_SHRINK,
		   standard_size/2, standard_size/4);

  g_signal_connect(G_OBJECT(ok_button_p), "clicked",
		   G_CALLBACK(PromptDialogCB::prompt_selected), this);
  g_signal_connect(G_OBJECT(cancel_button_p), "clicked",
		   G_CALLBACK(PromptDialogCB::prompt_selected), this);

  GTK_WIDGET_SET_FLAGS(ok_button_p, GTK_CAN_DEFAULT);
  GTK_WIDGET_SET_FLAGS(cancel_button_p, GTK_CAN_DEFAULT);

  gtk_container_add(GTK_CONTAINER(get_win()), GTK_WIDGET(table_p));
  
  gtk_window_set_type_hint(get_win(), GDK_WINDOW_TYPE_HINT_DIALOG);

  gtk_container_set_border_width(GTK_CONTAINER(get_win()), standard_size/2);

  gtk_widget_grab_focus(ok_button_p);

  gtk_window_set_position(get_win(), GTK_WIN_POS_CENTER_ON_PARENT);
  gtk_window_set_resizable(get_win(), false);

  gtk_widget_show_all(GTK_WIDGET(get_win()));
}

int PromptDialog::get_exec_val(void) const {
  return result;
}

void PromptDialog::on_delete_event(void) {
  gtk_widget_hide_all(GTK_WIDGET(get_win()));
  result = false;
  rejected();
  close();
}
