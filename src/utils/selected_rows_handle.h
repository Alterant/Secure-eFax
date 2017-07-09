/* Copyright (C) 2004 and 2005 Chris Vine

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

#ifndef SELECTED_ROWS_HANDLE_H
#define SELECTED_ROWS_HANDLE_H

#include <list>

#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeview.h>

#include "tree_row_reference_handle.h"
#include "tree_path_handle.h"

typedef std::list<TreePathSharedHandle> RowPathList;
typedef std::list<TreeRowRefSharedHandle> RowRefList;

namespace { // we put the function in anonymous namespace in selected_rows_handle.cpp
            // so it is not exported at link time
namespace SelectedRowsHandleCB {
  extern "C" void selected_rows_handle_fill_cb(GtkTreeModel*, GtkTreePath*, GtkTreeIter*, void*);
}
}


class SelectedRowsHandle {
  RowPathList path_list;
public:
  friend void SelectedRowsHandleCB::selected_rows_handle_fill_cb(GtkTreeModel*,
								 GtkTreePath*,
								 GtkTreeIter*,
								 void*);
  void fill(const GtkTreeView* tree_view_p);
  RowPathList::size_type size(void) const {return path_list.size();}
  bool is_empty(void) const {return path_list.empty();}

  // get_ref_list() should be called and the result stored before anything is done to
  // the tree view which invalidates the selected paths it provides (row references
  // are not invalidated by things done to the tree view, but paths can be)
  // this method will change the RowRefList argument - it will empty it of
  // any previous selection and then fill it with the current one
  void get_ref_list(GtkTreeModel*, RowRefList&) const;

  // the method front() can also be invalidated by things done to the tree
  // view as it returns a path. If the tree view might change use get_ref_list()
  // first and then call RowRefList::front() with respect to that list
  TreePathSharedHandle front(void) const {return path_list.front();}

  SelectedRowsHandle() {}

  // if a Gtk::TreeView object is passed to the constructor, then the
  // path list will be filled immediately without the need to call
  // SelectedRowsHandle::fill()
  SelectedRowsHandle(const GtkTreeView* tree_view_p) {fill(tree_view_p);}
};

#endif
