INCLUDES = -I. $(LIBGNOME_APPLET_CFLAGS)

lib_LTLIBRARIES = libgnome-applet.la

libgnome_applet_la_SOURCES =		\
	gnome-applet.h			\
	gnome-applet.c			\
	gnome-applet-config.c		\
	gnome-applet-config.h		\
	gnome-applet-background-type.c	\
	gnome-applet-background-type.h	\
	gnome-applet-orientation.c	\
	gnome-applet-orientation.h	\
	gnome-applet-label.c		\
	gnome-applet-label.h		\
	gnome-applet-layout-box.c	\
	gnome-applet-layout-box.h	\
	gnome-applet-marshal.c		\
	gnome-applet-marshal.h		\
	applet-server-client.h		\
	applet-client.h

BUILT_SOURCES = \
	gnome-applet-orientation.c	\
	gnome-applet-background-type.c	\
	gnome-applet-marshal.c		\
	gnome-applet-marshal.h		\
	applet-server-client.h		\
	applet-client.h

EXTRA_DIST = \
	applet-server-service.xml		\
	applet-service.xml			\
	glib-mkenums.template			\
	libgnome-applet.pc.in			\
	libgnome-applet-uninstalled.pc.in	\
	libgnome-applet.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgnome-applet.pc

libgnome_appletincludedir = $(includedir)/libgnome-applet

libgnome_appletinclude_HEADERS =        \
	libgnome-applet.h		\
	gnome-applet.h			\
	gnome-applet-label.h		\
	gnome-applet-layout-box.h	\
	gnome-applet-config.h		\
	gnome-applet-orientation.h	\
	gnome-applet-background-type.h

CLEANFILES = \
	$(BUILT_SOURCES)

applet-client.h: applet-service.xml
	dbus-binding-tool --prefix=applet --mode=glib-client --output=$@ $<

applet-server-client.h: applet-server-service.xml
	dbus-binding-tool --prefix=applet_server --mode=glib-client --output=$@ $<

gnome-applet-orientation.c: gnome-applet-orientation.h
	glib-mkenums --template glib-mkenums.template $^ > $@

gnome-applet-background-type.c: gnome-applet-background-type.h
	glib-mkenums --template glib-mkenums.template $^ > $@

gnome-applet-marshal.h: gnome-applet-marshal.list
	glib-genmarshal --prefix=gnome_applet_marshal --header $^ > $@

gnome-applet-marshal.c: gnome-applet-marshal.list
	echo '#include "gnome-applet-marshal.h"' > $@
	glib-genmarshal --prefix=gnome_applet_marshal --body $^ >> $@
