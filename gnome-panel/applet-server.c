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

#include <dbus/dbus-glib-bindings.h>
#include <glib-object.h>

#include "panel-profile.h"
#include "gilded-star.h"

#include "applet-server.h"

#define APPLET_SERVER_TYPE applet_server_get_type()

typedef GObject AppletServer;
typedef GObjectClass AppletServerClass;

static GType applet_server_get_type (void);

G_DEFINE_TYPE (AppletServer, applet_server, G_TYPE_OBJECT);

static void
applet_server_create_applet (AppletServer *as, const char *ignore_for_now,
                             DBusGMethodInvocation *context)
{
  PanelToplevel *toplevel;
  PanelWidget *widget;
  GSList *toplevels;
  char *id;

  printf ("someone wants a '%s'\n", ignore_for_now);

  toplevels = panel_toplevel_list_toplevels ();
  toplevel = toplevels->data;
  widget = panel_toplevel_get_panel_widget (toplevel);

  id = panel_profile_prepare_applet (toplevel, 1270, FALSE, ignore_for_now);
printf ("server got id '%s'\n", id);
  gilded_star_provide_dbus_context (id, context);
  panel_profile_add_to_list (PANEL_GCONF_GILDED_STARS, id);
  g_free (id);
}

#include "applet-server-service.h"

static void
applet_server_init (AppletServer *as)
{
  DBusGConnection *dbus;

  dbus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
  dbus_g_connection_register_g_object (dbus, "/ca/desrt/AppletServer",
                                       G_OBJECT (as));
}

static void
applet_server_class_init (AppletServerClass *class)
{
  DBusGConnection *dbus;
  DBusGProxy *proxy;
  unsigned int ret;

  /* XXX check for error */
  dbus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

  proxy = dbus_g_proxy_new_for_name (dbus,
                                     DBUS_SERVICE_DBUS,
                                     DBUS_PATH_DBUS,
                                     DBUS_INTERFACE_DBUS);

  org_freedesktop_DBus_request_name (proxy,
                                     "ca.desrt.AppletServer",
                                     0, &ret, NULL);

  dbus_g_object_type_install_info (APPLET_SERVER_TYPE,
                                   &dbus_glib_applet_server_object_info);
}

void
applet_server_initialise (void)
{
  g_object_new (APPLET_SERVER_TYPE, NULL);
}
