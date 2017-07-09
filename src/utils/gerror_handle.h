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

#ifndef GERROR_HANDLE_H
#define GERROR_HANDLE_H

#include <glib/gerror.h>
#include "shared_handle.h"

class GerrorFree {
public:
  void operator()(GError* obj_p) {
    if (obj_p) {
      g_error_free(obj_p);
    }
  }
};

typedef ScopedHandle<GError*, GerrorFree> GerrorScopedHandle;

#endif
