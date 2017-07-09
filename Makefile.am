## Process this file with automake to produce Makefile.in

SUBDIRS = src efax po efax-gtk-faxfilter

desktopdir = $(datadir)/applications

desktop_DATA = efax-gtk.desktop

sysconf_DATA = efax-gtkrc

dist_man_MANS = efax-gtk.1

release: dist
	mv @PACKAGE@-@VERSION@.tar.gz @PACKAGE@-@VERSION@.src.tgz 

snapshot: dist
	mv @PACKAGE@-@VERSION@.tar.gz @PACKAGE@2-`date +%y%m%d`.src.tgz 

EXTRA_DIST = efax-gtkrc efax-gtk.spec config.rpath BUGS efax-gtk.desktop mail_fax print_fax
