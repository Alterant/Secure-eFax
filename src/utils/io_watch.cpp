/* Copyright (C) 2005 to 2007 Chris Vine

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

#include "io_watch.h"

struct WatchSource {
  GSource source;
  GPollFD poll_fd;
  GIOCondition io_condition;
  sigc::slot<bool>* dispatch_slot_p;
};

extern "C" {
  gboolean io_watch_prepare_func(GSource*, gint*);
  gboolean io_watch_check_func(GSource*);
  gboolean io_watch_dispatch_func(GSource*, GSourceFunc, void*);
  void io_watch_finalize_func(GSource*);
}

namespace { // callbacks for internal use only

gboolean io_watch_prepare_func(GSource*, gint* timeout_p) {

  *timeout_p = -1;
  return false; // we want the file descriptor to be polled
}

gboolean io_watch_check_func(GSource* source_p) {
  // reinterpret_cast<>() is guaranteed to give the correct result here as the
  // address of WatchSource::source must be the same as the address of the
  // instance of the WatchSource struct of which it is the first member as
  // both are PODSs
  WatchSource* watch_source_p = reinterpret_cast<WatchSource*>(source_p);

  // what we have got
  gushort poll_condition = watch_source_p->poll_fd.revents;
  // what we are looking for
  gushort watch_condition = watch_source_p->io_condition;

  // return true if we have what we are looking for
  return (poll_condition & watch_condition); 
}

gboolean io_watch_dispatch_func(GSource* source_p, GSourceFunc, void*) {
  // reinterpret_cast<>() is guaranteed to give the correct result here as the
  // address of WatchSource::source must be the same as the address of the
  // instance of the WatchSource struct of which it is the first member as
  // both are PODSs
  WatchSource* watch_source_p = reinterpret_cast<WatchSource*>(source_p);

  // we are not interested in the GSourceFunc argument here as we have never
  // called g_source_set_callback()
  return (*watch_source_p->dispatch_slot_p)();
}

void io_watch_finalize_func(GSource* source_p) {

  // reinterpret_cast<>() is guaranteed to give the correct result here as the
  // address of WatchSource::source must be the same as the address of the
  // instance of the WatchSource struct of which it is the first member as
  // both are PODSs
  WatchSource* watch_source_p = reinterpret_cast<WatchSource*>(source_p);
  
  delete watch_source_p->dispatch_slot_p;
  watch_source_p->dispatch_slot_p = 0;
}

GSourceFuncs io_watch_source_funcs = {
  io_watch_prepare_func,
  io_watch_check_func,
  io_watch_dispatch_func,
  io_watch_finalize_func
};

} // anonymous namespace

guint start_iowatch(int fd, const sigc::slot<bool>& callback,
		    GIOCondition io_condition, GMainContext* context_p) {

  // context_p has a default value of NULL which will create the watch
  // in the default program main context

  GSource* source_p = g_source_new(&io_watch_source_funcs, sizeof(WatchSource));
  // reinterpret_cast<>() is guaranteed to give the correct result here as the
  // address of WatchSource::source must be the same as the address of the
  // instance of the WatchSource struct of which it is the first member as
  // both are PODSs
  WatchSource* watch_source_p = reinterpret_cast<WatchSource*>(source_p);
  watch_source_p->poll_fd.fd = fd;
  watch_source_p->poll_fd.events = io_condition;
  watch_source_p->poll_fd.revents = 0;
  watch_source_p->io_condition = io_condition;
  watch_source_p->dispatch_slot_p = new sigc::slot<bool>(callback);

  // connect the source object to its polling object  
  g_source_add_poll(source_p, &watch_source_p->poll_fd);

  // attach the source to the relevant main context
  guint id = g_source_attach(source_p, context_p);

  // g_source_attach() will add a reference count to the GSource object
  // so we unreference it here so that the callback returning false or
  // calling g_source_remove() on the return value of this function will
  // finalize/destroy the GSource object
  g_source_unref(source_p);

  return id;
}
