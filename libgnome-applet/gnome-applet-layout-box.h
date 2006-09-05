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

#ifndef _gnome_applet_layout_box_h_
#define _gnome_applet_layout_box_h_

#include <gtk/gtk.h>

#include "gnome-applet-orientation.h"
#include "gnome-applet-label.h"

#define GNOME_TYPE_APPLET_LAYOUT_BOX gnome_applet_layout_box_get_type()

#define GNOME_APPLET_LAYOUT_BOX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  GNOME_TYPE_APPLET_LAYOUT_BOX, GnomeAppletLayoutBox))

#define GNOME_APPLET_LAYOUT_BOX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  GNOME_TYPE_APPLET_LAYOUT_BOX, GnomeAppletLayoutBoxClass))

#define GNOME_IS_APPLET_LAYOUT_BOX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  GNOME_TYPE_APPLET_LAYOUT_BOX))

#define GNOME_IS_APPLET_LAYOUT_BOX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  GNOME_TYPE_APPLET_LAYOUT_BOX))

#define GNOME_APPLET_LAYOUT_BOX_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  GNOME_TYPE_APPLET_LAYOUT_BOX, GnomeAppletLayoutBoxClass))

typedef struct {
  GtkAlignment parent;
} GnomeAppletLayoutBox;

typedef struct {
  GtkAlignmentClass parent_class;
} GnomeAppletLayoutBoxClass;

GType gnome_applet_layout_box_get_type (void);

GnomeAppletLayoutBox *gnome_applet_layout_box_new (void);
void gnome_applet_layout_box_set_orientation (GnomeAppletLayoutBox *,
                                              GnomeAppletOrientation);
GtkImage *gnome_applet_layout_box_request_image (GnomeAppletLayoutBox *);
GnomeAppletLabel *gnome_applet_layout_box_request_label (GnomeAppletLayoutBox *);

#endif
