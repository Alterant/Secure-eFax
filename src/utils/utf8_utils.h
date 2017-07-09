/* Copyright (C) 2005 Chris Vine

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

#ifndef UTF8_UTILS_H
#define UTF8_UTILS_H

#include <string>
#include <glib/gerror.h>
#include <glib/gunicode.h>

#include "shared_handle.h"

namespace Utf8 {

struct ConversionError {
  std::string message;
  ConversionError(GError* error_p): message((const char*)error_p->message) {}
};

std::string filename_from_utf8(const std::string& input);

std::string filename_to_utf8(const std::string& input); 

std::string locale_from_utf8(const std::string& input);

std::string locale_to_utf8(const std::string& input); 

inline std::string to_lower(const std::string& text) {
  GcharScopedHandle g_str_h(g_utf8_strdown(text.data(), text.size()));
  return std::string(g_str_h.get());
}

inline bool validate(const std::string& text) {
  return g_utf8_validate(text.data(), text.size(), 0);
}

// Reassembler is a functor class which takes in a partially formed
// UTF-8 string and returns a null terminated string comprising such
// of the input string (after inserting, at the beginning, any
// partially formed UTF-8 character which was at the end of the input
// string passed in the previous call to the functor) as forms
// complete UTF-8 characters (storing any partial character at the end
// for the next call to the functor).  If the input string contains
// invalid UTF-8 after adding any stored previous part character
// (apart from any partially formed character at the end of the input
// string) then operator() will return a null SharedHandle<char*>
// object (that is, SharedHandle<char*>::get() will return 0).  Such
// input will not be treated as invalid if it consists only of a
// single partly formed UTF-8 character which could be valid if
// further bytes were received and added to it.  In that case the
// returned SharedHandle<char*> object will contain an allocated
// string of zero length (apart from the terminating 0 character),
// rather than a NULL pointer.

// this enables UTF-8 strings to be sent over pipes, sockets, etc and
// displayed in a GTK+ object at the receiving end

// note that for efficiency reasons the memory held in the returned
// SharedHandle<char*> object may be greater than length of the
// null-terminated string that is contained in that memory: just let
// the SharedHandle<char*> object manage the memory, and use the
// contents like any other null-terminated string

class Reassembler {
  size_t stored;
  const static size_t buff_size = 6;
  char buffer[buff_size];
  char* join_buffer(const char*, size_t);
public:
  // the second argument is the number of bytes not the number of utf8 characters
  SharedHandle<char*> operator()(const char*, size_t);

  size_t get_stored(void) const {return stored;}
  void reset(void) {stored = 0;}
  Reassembler(void): stored(0) {}
};

} // namespace Utf8

#endif
