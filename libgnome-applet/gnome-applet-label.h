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

#ifndef _gnome_applet_label_h_
#define _gnome_applet_label_h_

#include <dbus/dbus-glib-bindings.h>
#include <gtk/gtk.h>

#define GNOME_TYPE_APPLET_LABEL gnome_applet_label_get_type()

#define GNOME_APPLET_LABEL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                   GNOME_TYPE_APPLET_LABEL, GnomeAppletLabel))

#define GNOME_APPLET_LABEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                GNOME_TYPE_APPLET_LABEL, GnomeAppletLabelClass))

#define GNOME_IS_APPLET_LABEL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                   GNOME_TYPE_APPLET_LABEL))

#define GNOME_IS_APPLET_LABEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                GNOME_TYPE_APPLET_LABEL))

#define GNOME_APPLET_LABEL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                  GNOME_TYPE_APPLET_LABEL, GnomeAppletLabelClass))

typedef struct _GnomeAppletLabelPrivate GnomeAppletLabelPrivate;
typedef struct _GnomeAppletLabelClass GnomeAppletLabelClass;
typedef struct _GnomeAppletLabel GnomeAppletLabel;

struct _GnomeAppletLabel
{
  GtkContainer parent;
};

struct _GnomeAppletLabelClass
{
  GtkContainerClass parent;
};

GtkWidget *gnome_applet_label_new (const char *text);
GType gnome_applet_label_get_type (void);
void gnome_applet_label_set_text (GnomeAppletLabel *gal, const char *text);
void gnome_applet_label_set_transparency_flag (GnomeAppletLabel *gal, int type);
void gnome_applet_label_set_sideways (GnomeAppletLabel *gal, gboolean sideways);
int gnome_applet_label_get_length (GnomeAppletLabel *gal);
int gnome_applet_label_get_thickness (GnomeAppletLabel *gal);

#endif
