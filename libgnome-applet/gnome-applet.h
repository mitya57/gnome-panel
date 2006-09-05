/*
 * Copyright © 2006 Ryan Lortie
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Library General 
 * Public License as published by the Free Software Foundation.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * Authors:
 *     Ryan Lortie <desrt@desrt.ca>
 */

#ifndef _gnome_applet_h_
#define _gnome_applet_h_

#include <dbus/dbus-glib-bindings.h>
#include <gtk/gtk.h>

#include "gnome-applet-background-type.h"
#include "gnome-applet-orientation.h"

#define GNOME_TYPE_APPLET gnome_applet_get_type()

#define GNOME_APPLET(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                   GNOME_TYPE_APPLET, GnomeApplet))

#define GNOME_APPLET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                GNOME_TYPE_APPLET, GnomeAppletClass))

#define GNOME_IS_APPLET(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                   GNOME_TYPE_APPLET))

#define GNOME_IS_APPLET_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                GNOME_TYPE_APPLET))

#define GNOME_APPLET_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                  GNOME_TYPE_APPLET, GnomeAppletClass))

typedef struct _GnomeAppletPrivate GnomeAppletPrivate;
typedef struct _GnomeAppletClass GnomeAppletClass;
typedef struct _GnomeApplet GnomeApplet;

struct _GnomeApplet
{
  GtkPlug parent;
};

struct _GnomeAppletClass
{
  GtkPlugClass parent;
};

GnomeApplet *gnome_applet_new (void);
GType gnome_applet_get_type (void);

GtkContainer *gnome_applet_get_drop_container (GnomeApplet *ga);
void gnome_applet_add_dropdown_widget (GnomeApplet *ga, GtkWidget *widget);

#define GNOME_APPLET_MAIN_INSTANTIATE(instfunction) \
  int \
  main (int argc, char **argv) \
  { \
    void *widget; \
    \
    gtk_init (&argc, &argv); \
    \
    widget = instfunction (argv[0]?argv[1]:NULL); \
    gtk_widget_show (widget); \
    \
    gtk_main (); \
    return 0; \
  }

#define GNOME_APPLET_DEFINE(Type, type, memb...) \
    typedef struct { GnomeApplet parent; memb } Type; \
    typedef struct { GnomeAppletClass parent; } Type##Class; \
    G_DEFINE_TYPE (Type, type, GNOME_TYPE_APPLET); \
    static void type##_initialise (Type *); \
    GObject *type##_constructor (GType t, guint n, GObjectConstructParam *p) \
    { \
      void *obj; obj = G_OBJECT_CLASS(type##_parent_class)->constructor (t, n, p); \
      type##_initialise (obj); \
      return obj; \
    } \
    void type##_init (Type *x) { } \
    void type##_class_init (Type##Class *class) \
    { \
      G_OBJECT_CLASS (class)->constructor = type##_constructor; \
    } \
    Type * type##_new (const char *id) { \
      return g_object_new (type##_get_type (), "applet-id", id, NULL); } \
    GNOME_APPLET_MAIN_INSTANTIATE (type##_new)

void gnome_applet_set_image_from_file (GnomeApplet *ga, const char *filename);
void gnome_applet_set_image_from_pixmap (GnomeApplet *ga, GdkPixmap *pixmap,
                                                          GdkBitmap *mask);
void gnome_applet_set_image_from_pixbuf (GnomeApplet *ga, GdkPixbuf *pixbuf);
void gnome_applet_set_image_from_stock (GnomeApplet *ga, const char *id);
void gnome_applet_set_image_from_icon_name (GnomeApplet *ga, const char *id);

void gnome_applet_set_text (GnomeApplet *ga, const char *label);
void gnome_applet_setf_text (GnomeApplet *ga, const char *fmt, ...);

gboolean gnome_applet_config_get (GnomeApplet *, const char *, GValue *);
gboolean gnome_applet_config_set (GnomeApplet *, const char *, const GValue *);

GnomeAppletBackgroundType gnome_applet_get_background_type (GnomeApplet *ga);
GnomeAppletOrientation gnome_applet_get_orientation (GnomeApplet *ga);

void gnome_applet_set_has_handle (GnomeApplet *ga, gboolean handle);
gboolean gnome_applet_get_has_handle (GnomeApplet *ga);

void gnome_applet_set_persist (GnomeApplet *ga, gboolean persist);

void gnome_applet_set_name (GnomeApplet *applet, const char *name);

void gnome_applet_add_menu_item (GnomeApplet *applet, const char *stock,
                                 const char *text, gpointer callback,
                                 gpointer user_data, GDestroyNotify unref);


#endif
