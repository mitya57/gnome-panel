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

#ifndef _gnome_applet_background_type_h_
#define _gnome_applet_background_type_h_

#include <glib-object.h>

typedef enum
{
  GNOME_APPLET_BACKGROUND_TYPE_PLAIN,
  GNOME_APPLET_BACKGROUND_TYPE_TRANSPARENT
} GnomeAppletBackgroundType;

#define GNOME_TYPE_APPLET_BACKGROUND_TYPE gnome_applet_background_type_get_type ()
GType gnome_applet_background_type_get_type (void);

#endif
