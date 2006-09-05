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

#ifndef _gnome_applet_orientation_h_
#define _gnome_applet_orientation_h_

#include <glib-object.h>

typedef enum
{
  GNOME_APPLET_ORIENTATION_TOP,
  GNOME_APPLET_ORIENTATION_BOTTOM,
  GNOME_APPLET_ORIENTATION_LEFT,
  GNOME_APPLET_ORIENTATION_RIGHT
} GnomeAppletOrientation;

#define GNOME_TYPE_APPLET_ORIENTATION gnome_applet_orientation_get_type ()
GType gnome_applet_orientation_get_type (void);

static inline gboolean
gnome_applet_orientation_is_horizontal (GnomeAppletOrientation o)
{
  return o == GNOME_APPLET_ORIENTATION_TOP ||
         o == GNOME_APPLET_ORIENTATION_BOTTOM;
}

static inline gboolean
gnome_applet_orientation_is_vertical (GnomeAppletOrientation o)
{
  return o == GNOME_APPLET_ORIENTATION_LEFT ||
         o == GNOME_APPLET_ORIENTATION_RIGHT;
}

#endif
