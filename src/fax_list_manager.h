/* Copyright (C) 2001 to 2005 Chris Vine

This program is distributed under the General Public Licence, version 2.
For particulars of this and relevant disclaimers see the file
COPYING distributed with the source files.

*/

#ifndef FAX_LIST_MANAGER_H
#define FAX_LIST_MANAGER_H

#include "prog_defs.h"

#include <string>
#include <map>
#include <set>
#include <fstream>
#include <utility>

#include <gtk/gtkwidget.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkenums.h>

#include <gdk/gdkpixbuf.h>
#include <glib/gtypes.h>

#include <sigc++/sigc++.h>

#include "utils/shared_handle.h"
#include "utils/gobj_handle.h"
#include "utils/tree_row_reference_handle.h"
#include "utils/selected_rows_handle.h"


typedef std::map<TreeRowRefSharedHandle, GobjHandle<GtkTreeModel> > FolderRowToFaxModelMap;
typedef std::map<std::string, TreeRowRefSharedHandle> PathToFolderRowMap;

/* The following typedefs are in selected_rows_handle.h
typedef std::list<TreePathSharedHandle> RowPathList;
typedef std::list<TreeRowRefSharedHandle> RowRefList;
*/

namespace FaxListEnum {
  enum Mode {received = 0, sent};
}

class FolderNameValidator {
  std::set<std::string> folder_set;
public:
  void clear(void) {folder_set.clear();}
  void insert_folder_name(const std::string& name) {folder_set.insert(name);}
  void erase_folder_name(const std::string& name) {folder_set.erase(name);}
  std::pair<bool, std::string> validate(const std::string&);
};

namespace { // we put the functions in anonymous namespace in fax_list_manager.cpp
            // so they are not exported at link time

namespace FaxListManagerCB {
  extern "C" {
    void folder_tree_view_drag_begin(GtkWidget*, GdkDragContext*, void*);
    void fax_tree_view_drag_begin(GtkWidget*, GdkDragContext*, void*);
    gboolean drag_motion(GtkWidget*, GdkDragContext*, gint, gint, guint, void*);
    gboolean folder_tree_view_motion_notify(GtkWidget*, GdkEventMotion*, void*);
    gboolean fax_tree_view_motion_notify(GtkWidget*, GdkEventMotion*, void*);
    void drag_data_get(GtkWidget*, GdkDragContext*, GtkSelectionData*,
		       guint, guint, void*);
    void drag_data_received(GtkWidget*, GdkDragContext*, gint, gint,
			    GtkSelectionData*, guint, guint, void*);
    void fax_selected(GtkTreeSelection*, void*);
    void folder_selected(GtkTreeSelection*, void*);
    gboolean write_path_timer_event(void*);
    void toggle_sort_type(GtkTreeViewColumn*, void*);
  } // extern "C"
}   // namespace FaxListManagerCB
}   // anonymous namespace

class FaxListManager: public sigc::trackable {

  static bool fax_received_list_main_iteration;
  static bool fax_sent_list_main_iteration;

  static GtkSortType list_sort_type[FaxListEnum::sent + 1];

  FaxListEnum::Mode mode;
  guint timer_tag;

  static const int target_size = 2;
  ScopedHandle<char*> target_0;
  ScopedHandle<char*> target_1;
  GtkTargetEntry target_array[target_size];

  bool folder_drag_source_enabled;
  bool fax_drag_source_enabled;
  GtkTreeIter folder_drag_source_row_iter;
  RowRefList fax_drag_source_row_refs;
  bool drag_is_fax;
  
  GtkTreeView* folder_tree_view_p;
  GtkTreeView* fax_tree_view_p;
  GtkTreeViewColumn* fax_date_column_p;
  GobjHandle<GtkTreeModel> folder_tree_store_h;
  GobjHandle<GtkTreeModel> fax_base_model_h;
  FolderRowToFaxModelMap folder_row_to_fax_model_map;

  GobjHandle<GdkPixbuf> folder_icon_h;
  GobjHandle<GdkPixbuf> trash_icon_h;

  TreeRowRefSharedHandle trash_row_ref_h;

  FolderNameValidator folder_name_validator;

  std::string get_pathname_for_folder(const GtkTreeIter*);
  void populate_fax_list(void);
  bool get_folders(PathToFolderRowMap&, const std::string&);
  void insert_fax_on_populate(const PathToFolderRowMap&, std::ifstream&,
			      const std::string&, const std::string&);
  void move_fax(const GtkTreeIter*);
  void move_folder(GtkTreeIter*, GtkTreeViewDropPosition);
  void move_child_folders_for_level(GtkTreeIter*, GtkTreeIter*);
  bool is_valid_drop_path(const std::string&, const std::string&);
  void display_faxes(void);
  void write_path(void);
  void write_paths_for_level(const GtkTreeIter*, std::ofstream&);
  std::string convert_faxname_to_date(const std::string&);

public:
  friend void FaxListManagerCB::folder_tree_view_drag_begin(GtkWidget*, GdkDragContext*, void*);
  friend void FaxListManagerCB::fax_tree_view_drag_begin(GtkWidget*, GdkDragContext*, void*);
  friend gboolean FaxListManagerCB::drag_motion(GtkWidget*, GdkDragContext*, gint, gint, guint, void*);
  friend gboolean FaxListManagerCB::folder_tree_view_motion_notify(GtkWidget*, GdkEventMotion*, void*);
  friend gboolean FaxListManagerCB::fax_tree_view_motion_notify(GtkWidget*, GdkEventMotion*, void*);
  friend void FaxListManagerCB::drag_data_get(GtkWidget*, GdkDragContext*, GtkSelectionData*,
					      guint, guint, void*);
  friend void FaxListManagerCB::drag_data_received(GtkWidget*, GdkDragContext*, gint, gint,
						   GtkSelectionData*, guint, guint, void*);
  friend void FaxListManagerCB::fax_selected(GtkTreeSelection*, void*);
  friend void FaxListManagerCB::folder_selected(GtkTreeSelection*, void*);
  friend gboolean FaxListManagerCB::write_path_timer_event(void*);
  friend void FaxListManagerCB::toggle_sort_type(GtkTreeViewColumn*, void*);

  sigc::signal0<void> selection_notify;

  void insert_folder_tree_view(GtkContainer* container_p) {
    gtk_container_add(container_p, GTK_WIDGET(folder_tree_view_p));
  }
  void insert_fax_tree_view(GtkContainer* container_p) {
    gtk_container_add(container_p, GTK_WIDGET(fax_tree_view_p));
  }

  void delete_fax(void);
  void delete_folder(void);
  std::pair<bool, std::string> is_folder_name_valid(const std::string& folder_name) {
    return folder_name_validator.validate(folder_name);
  }
  void make_folder(const std::string&, bool test_valid = true);
  void describe_fax(const std::string&);
  void empty_trash_folder(void);
  void insert_new_fax_in_base(const std::string&, const std::string&);

  RowPathList::size_type is_fax_selected(void);
  bool is_folder_selected(void);
  bool is_selected_folder_empty(void);
  bool is_selected_folder_permanent(void);
  bool show_trash_folder_icon(void);

  bool are_selected_faxes_in_trash_folder(void);
  void move_selected_faxes_to_trash_folder(void);

  static bool is_fax_received_list_main_iteration(void) {return fax_received_list_main_iteration;}
  static bool is_fax_sent_list_main_iteration(void) {return fax_sent_list_main_iteration;}

  static GtkSortType get_received_list_sort_type(void) {return list_sort_type[FaxListEnum::received];}
  static GtkSortType get_sent_list_sort_type(void) {return list_sort_type[FaxListEnum::sent];}
  static void set_received_list_sort_type(GtkSortType val) {list_sort_type[FaxListEnum::received] = val;}
  static void set_sent_list_sort_type(GtkSortType val) {list_sort_type[FaxListEnum::sent] = val;}

  GcharSharedHandle get_fax_number(void);
  GcharSharedHandle get_fax_description(void);
  GcharSharedHandle get_folder_name(void);

  FaxListManager(FaxListEnum::Mode);
  ~FaxListManager(void);
};

#endif
