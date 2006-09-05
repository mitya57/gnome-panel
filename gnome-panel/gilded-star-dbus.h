#ifndef _gilded_star_dbus_h_
#define _gilded_star_dbus_h_

/* the following declarations are only visible to the dbus binding */

static int gilded_star_request_drop_socket (GildedStar *);
static int gilded_star_get_xid (GildedStar *);
static gboolean gilded_star_give_xid (GildedStar *, int xid);

static GildedStarBackgroundType gilded_star_get_background_type (GildedStar *);
static GildedStarOrientation gilded_star_get_orientation (GildedStar *);

static void gilded_star_set_life (GildedStar *star, GildedStarLifetime life);
static void gilded_star_set_name (GildedStar *star, const char *name);
static void gilded_star_set_icon (GildedStar *star, const char *name);

static int gilded_star_add_menu_item (GildedStar *, const char *stock,
                                       const char *label);
static gboolean gilded_star_config_set (GildedStar *star, const char *name, const GValue *value, GError *error);
static gboolean gilded_star_config_get (GildedStar *star, const char *name, GValue *value, GError *error);
static gboolean gilded_star_config_touch (GildedStar *star, const char *name);

#include "applet-service.h"

#endif
