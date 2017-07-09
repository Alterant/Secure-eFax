/* Copyright (C) 2005 and 2006 Chris Vine

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

#ifndef IO_WATCH_H
#define IO_WATCH_H

/* The start_iowatch() function connects a Unix file descriptor to an
   event loop owned by a GMainContext object (normally the main
   program loop).  It saves the overhead of having to construct a
   GIOChannel object where the only thing wanted is to execute a
   callback when there is something to be read from a pipe, fifo or
   socket or a pipe or fifo can be written to.

   As this method has been written, an iowatch (WatchSource) object
   should not check for both input and output (in other words pass
   G_IO_IN or G_IO_OUT to the GIOCondition argument in
   start_iowatch(), but not a bitwise-ORed version of both).  If the
   same descriptor is used for both then there should be separate
   watches for reading and writing.  This could be changed however by
   making the callback slot take a GIOCondition argument, and sending
   the value of watch_source_p->poll_fd.revents to the callback
   function in io_watch_dispatch_func() so it can inspect it and
   decide whether the file descriptor is available for reading,
   writing or both.

   G_IO_IN can be bitwise-ORed with G_IO_HUP, and should be if the
   callback has the task of cleaning up if EOF is reached (see
   http://www.greenend.org.uk/rjk/2001/06/poll.html ), which is
   detected by read() returning 0.  A cast will be required to do this
   for the third argument of start_iowatch() (that is, pass
   GIOCondition(G_IO_IN | G_IO_HUP)).

   start_iowatch() returns the id of the WatchSource object (derived
   from GSource) in the main program context to which it is attached.
   The watch will be ended, and all resources connected with it
   deleted, either by the callback returning false or by calling
   g_source_remove() on the id returned by start_iowatch().  The
   callback will automatically return false if it is a non-static
   member function of an object derived from sigc::trackable and it is
   called after that object has been destroyed (ie it is an object
   which has gone out of scope or had delete called on it), so in that
   case there is no need to call g_source_remove() (and if the
   callback has already returned false, g_source_remove() should not
   afterwards be called on the tag id value returned by
   start_iowatch() in case it has been reused by the relevant
   GMainContext object in the meantime).
*/

#include "prog_defs.h"   // for sigc::slot

#include <glib/gmain.h>
#include <glib/gtypes.h>
#include <glib/giochannel.h>

#include <sigc++/sigc++.h>

// the default value for the last argument passed to start_iowatch() (the
// GMainContext* argument) of NULL will cause the watch to be attached
// to the main program loop - this is almost always what is wanted
guint start_iowatch(int fd, const sigc::slot<bool>& callback,
		    GIOCondition io_condition, GMainContext* context_p = 0);

#endif
