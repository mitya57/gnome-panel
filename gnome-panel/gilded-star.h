/*
 * Copyright (C) 2006 Ryan Lortie
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors:
 *	Ryan Lortie <desrt@desrt.ca>
 */

#ifndef _GILDED_STAR_H_
#define _GILDED_STAR_H_

#include <gtk/gtk.h>

#define GILDED_STAR_TYPE gilded_star_get_type()

#define GILDED_STAR(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
     GILDED_STAR_TYPE, GildedStar))

#define GILDED_STAR_CLASS(class) \
    (G_TYPE_CHECK_CLASS_CAST ((class), \
     GILDED_STAR_TYPE, GildedStarClass))

#define IS_GILDED_STAR(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GILDED_STAR_TYPE))

#define IS_GILDED_STAR_CLASS(class) \
    (G_TYPE_CHECK_CLASS_TYPE ((class), GILDED_STAR_TYPE))

#define GILDED_STAR_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), \
     GILDED_STAR_TYPE, GildedStarClass))

typedef enum
{
  GILDED_STAR_BACKGROUND_NORMAL,
  GILDED_STAR_BACKGROUND_TRANSPARENT
} GildedStarBackgroundType;

typedef enum
{
  GILDED_STAR_ORIENTATION_TOP,
  GILDED_STAR_ORIENTATION_BOTTOM,
  GILDED_STAR_ORIENTATION_LEFT,
  GILDED_STAR_ORIENTATION_RIGHT
} GildedStarOrientation;

typedef enum
{
  GILDED_STAR_LIFETIME_NORMAL,
  GILDED_STAR_LIFETIME_TEMPORARY,
  GILDED_STAR_LIFETIME_PERSISTENT
} GildedStarLifetime;

typedef struct
{
  GtkSocket parent;
} GildedStar;

typedef struct
{
  GtkSocketClass parent;
} GildedStarClass;

GType gilded_star_get_type (void);

void gilded_star_set_panel_orientation (GildedStar *star, int);
void gilded_star_set_panel_background_type (GildedStar *star,
                                            PanelBackgroundType type);

void gilded_star_menu_item_callback (GildedStar *star, const char *item);

void gilded_star_provide_dbus_context (const char *id, void *context);
void gilded_star_load_from_gconf (PanelWidget *panel, gboolean locked,
                                  int position, const char *id);

#endif
