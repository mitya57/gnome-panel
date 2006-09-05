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

#include "gnome-applet-config.h"

void
gnome_applet_config_watch (GnomeApplet *applet, const char *key,
                           GnomeAppletConfigChangeFunc callback,
                           gpointer user_data, GDestroyNotify notify)
{
  gnome_applet_config_watch_with_marshaller (applet, key, NULL,
                                             callback, user_data, notify);
}

gboolean
gnome_applet_config_set_string (GnomeApplet *applet, const char *key,
                                const char *value)
{
  GValue gval = { 0, };

  g_value_init (&gval, G_TYPE_STRING);
  /* we can use set_static here since the string will
     live for longer than the life of the GValue. */
  g_value_set_static_string (&gval, value);
  return gnome_applet_config_set (applet, key, &gval);
}

gboolean
gnome_applet_config_get_string (GnomeApplet *applet, const char *key,
                                char **value)
{
  GValue gval = { 0, };

  gnome_applet_config_get (applet, key, &gval);

  if (!G_VALUE_HOLDS (&gval, G_TYPE_STRING))
  {
    *value = NULL;
    return FALSE;
  }

  *value = g_value_dup_string (&gval);
  g_value_unset (&gval);

  return TRUE;
}

char *
gnome_applet_config_get_string_default (GnomeApplet *applet, const char *key,
                                        const char *dvalue)
{
  char *value;

  if (!gnome_applet_config_get_string (applet, key, &value))
    value = g_strdup (dvalue);

  return value;
}

static void
gnome_applet_config_marshal_string (GnomeApplet *applet, const char *key,
                                    const GValue *value, gpointer callback,
                                    gpointer user_data)
{
  if (!G_VALUE_HOLDS (value, G_TYPE_STRING))
    return;

  ((GnomeAppletConfigStringChangeFunc)callback) (applet, key,
    g_value_get_string (value), user_data);
}

void
gnome_applet_config_watch_string (GnomeApplet *applet, const char *key,
                                  GnomeAppletConfigStringChangeFunc callback,
                                  gpointer user_data, GDestroyNotify notify)
{
  gnome_applet_config_watch_with_marshaller (applet, key,
    gnome_applet_config_marshal_string, callback, user_data, notify);
}


/* --- */

gboolean
gnome_applet_config_set_int (GnomeApplet *applet, const char *key, int value)
{
  GValue gval = { 0, };

  g_value_init (&gval, G_TYPE_INT);
  g_value_set_int (&gval, value);
  return gnome_applet_config_set (applet, key, &gval);
}

gboolean
gnome_applet_config_get_int (GnomeApplet *applet, const char *key, int *value)
{
  GValue gval = { 0, };

  gnome_applet_config_get (applet, key, &gval);

  if (!G_VALUE_HOLDS (&gval, G_TYPE_INT))
    return FALSE;

  *value = g_value_get_int (&gval);
  g_value_unset (&gval);

  return TRUE;
}

int
gnome_applet_config_get_int_default (GnomeApplet *applet, const char *key,
                                     int dvalue)
{
  int value;

  if (!gnome_applet_config_get_int (applet, key, &value))
    value = dvalue;

  return value;
}

static void
gnome_applet_config_marshal_int (GnomeApplet *applet, const char *key,
                                 const GValue *value, gpointer callback,
                                 gpointer user_data)
{
  if (!G_VALUE_HOLDS (value, G_TYPE_INT))
    return;

  ((GnomeAppletConfigIntChangeFunc)callback) (applet, key,
    g_value_get_int (value), user_data);
}

void
gnome_applet_config_watch_int (GnomeApplet *applet, const char *key,
                               GnomeAppletConfigIntChangeFunc callback,
                               gpointer user_data, GDestroyNotify notify)
{
  gnome_applet_config_watch_with_marshaller (applet, key,
    gnome_applet_config_marshal_int, callback, user_data, notify);
}

/* --- */

gboolean
gnome_applet_config_set_double (GnomeApplet *applet, const char *key,
                                double value)
{
  GValue gval = { 0, };

  g_value_init (&gval, G_TYPE_DOUBLE);
  g_value_set_double (&gval, value);
  return gnome_applet_config_set (applet, key, &gval);
}

gboolean
gnome_applet_config_get_double (GnomeApplet *applet, const char *key,
                                double *value)
{
  GValue gval = { 0, };

  gnome_applet_config_get (applet, key, &gval);

  if (!G_VALUE_HOLDS (&gval, G_TYPE_DOUBLE))
    return FALSE;

  *value = g_value_get_double (&gval);
  g_value_unset (&gval);

  return TRUE;
}

double
gnome_applet_config_get_double_default (GnomeApplet *applet, const char *key,
                                        double dvalue)
{
  double value;

  if (!gnome_applet_config_get_double (applet, key, &value))
    value = dvalue;

  return value;
}

static void
gnome_applet_config_marshal_double (GnomeApplet *applet, const char *key,
                                    const GValue *value, gpointer callback,
                                    gpointer user_data)
{
  if (!G_VALUE_HOLDS (value, G_TYPE_DOUBLE))
    return;

  ((GnomeAppletConfigDoubleChangeFunc)callback) (applet, key,
    g_value_get_double (value), user_data);
}

void
gnome_applet_config_watch_double (GnomeApplet *applet, const char *key,
                                  GnomeAppletConfigDoubleChangeFunc callback,
                                  gpointer user_data, GDestroyNotify notify)
{
  gnome_applet_config_watch_with_marshaller (applet, key,
    gnome_applet_config_marshal_double, callback, user_data, notify);
}

/* --- */

gboolean
gnome_applet_config_set_boolean (GnomeApplet *applet, const char *key,
                                 gboolean value)
{
  GValue gval = { 0, };

  g_value_init (&gval, G_TYPE_BOOLEAN);
  g_value_set_boolean (&gval, value);
  return gnome_applet_config_set (applet, key, &gval);
}

gboolean
gnome_applet_config_get_boolean (GnomeApplet *applet, const char *key,
                                 gboolean *value)
{
  GValue gval = { 0, };

  gnome_applet_config_get (applet, key, &gval);

  if (!G_VALUE_HOLDS (&gval, G_TYPE_BOOLEAN))
    return FALSE;

  *value = g_value_get_boolean (&gval);
  g_value_unset (&gval);

  return TRUE;
}

gboolean
gnome_applet_config_get_boolean_default (GnomeApplet *applet, const char *key,
                                         gboolean dvalue)
{
  gboolean value;

  if (!gnome_applet_config_get_boolean (applet, key, &value))
    value = dvalue;

  return value;
}

static void
gnome_applet_config_marshal_boolean (GnomeApplet *applet, const char *key,
                                     const GValue *value, gpointer callback,
                                     gpointer user_data)
{
  if (!G_VALUE_HOLDS (value, G_TYPE_BOOLEAN))
    return;

  ((GnomeAppletConfigBooleanChangeFunc)callback) (applet, key,
    g_value_get_boolean (value), user_data);
}

void
gnome_applet_config_watch_boolean (GnomeApplet *applet, const char *key,
                                   GnomeAppletConfigBooleanChangeFunc callback,
                                   gpointer user_data, GDestroyNotify notify)
{
  gnome_applet_config_watch_with_marshaller (applet, key,
    gnome_applet_config_marshal_boolean, callback, user_data, notify);
}
