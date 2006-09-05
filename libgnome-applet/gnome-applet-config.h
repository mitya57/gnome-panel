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

#ifndef _GNOME_APPLET_CONFIG_H_
#define _GNOME_APPLET_CONFIG_H_

#include "gnome-applet.h"


/* generic */
typedef void (*GnomeAppletConfigMarshalFunc)       (GnomeApplet *applet,
                                                    const char *key,
                                                    const GValue *value,
                                                    gpointer callback,
                                                    gpointer user_data);
void gnome_applet_config_watch_with_marshaller     (GnomeApplet *applet,
                                                    const char *key,
                                       GnomeAppletConfigMarshalFunc marshall,
                                                    gpointer callback,
                                                    gpointer user_data,
                                                    GDestroyNotify notify);
void gnome_applet_config_touch                     (GnomeApplet *applet,
                                                    const char *key);

/* GValue */
typedef void (*GnomeAppletConfigChangeFunc)        (GnomeApplet *applet,
                                                    const char *key,
                                                    const GValue *value,
                                                    gpointer user_data);
gboolean gnome_applet_config_get                   (GnomeApplet *applet,
                                                    const char *key,
                                                    GValue *value);
gboolean gnome_applet_config_set                   (GnomeApplet *applet,
                                                    const char *key,
                                                    const GValue *value);
void gnome_applet_config_watch                     (GnomeApplet *applet,
                                                    const char *key,
                                        GnomeAppletConfigChangeFunc callback,
                                                    gpointer user_data,
                                                    GDestroyNotify notify);

/* string */
typedef void (*GnomeAppletConfigStringChangeFunc)  (GnomeApplet *applet,
                                                    const char *key,
                                                    const char *value,
                                                    gpointer user_data);
gboolean gnome_applet_config_set_string            (GnomeApplet *applet,
                                                    const char *key,
                                                    const char *value);
gboolean gnome_applet_config_get_string            (GnomeApplet *applet,
                                                    const char *key,
                                                    char **value);
char * gnome_applet_config_get_string_default      (GnomeApplet *applet,
                                                    const char *key,
                                                    const char *dvalue);
void gnome_applet_config_watch_string              (GnomeApplet *applet,
                                                    const char *key,
                                  GnomeAppletConfigStringChangeFunc callback,
                                                    gpointer user_data,
                                                    GDestroyNotify notify);

/* int */
typedef void (*GnomeAppletConfigIntChangeFunc)     (GnomeApplet *applet,
                                                    const char *key,
                                                    int value,
                                                    gpointer user_data);
gboolean gnome_applet_config_set_int               (GnomeApplet *applet,
                                                    const char *key,
                                                    int value);
gboolean gnome_applet_config_get_int               (GnomeApplet *applet,
                                                    const char *key,
                                                    int *value);
int gnome_applet_config_get_int_default            (GnomeApplet *applet,
                                                    const char *key,
                                                    int dvalue);
void gnome_applet_config_watch_int                 (GnomeApplet *applet,
                                                    const char *key,
                                     GnomeAppletConfigIntChangeFunc callback,
                                                    gpointer user_data,
                                                    GDestroyNotify notify);

/* double */
typedef void (*GnomeAppletConfigDoubleChangeFunc)  (GnomeApplet *applet,
                                                    const char *key,
                                                    double value,
                                                    gpointer user_data);
gboolean gnome_applet_config_set_double            (GnomeApplet *applet,
                                                    const char *key,
                                                    double value);
gboolean gnome_applet_config_get_double            (GnomeApplet *applet,
                                                    const char *key,
                                                    double *value);
double gnome_applet_config_get_double_default      (GnomeApplet *applet,
                                                    const char *key,
                                                    double dvalue);
void gnome_applet_config_watch_double              (GnomeApplet *applet,
                                                    const char *key,
                                  GnomeAppletConfigDoubleChangeFunc callback,
                                                    gpointer user_data,
                                                    GDestroyNotify notify);

/* boolean */
typedef void (*GnomeAppletConfigBooleanChangeFunc) (GnomeApplet *applet,
                                                    const char *key,
                                                    gboolean value,
                                                    gpointer user_data);
gboolean gnome_applet_config_set_boolean           (GnomeApplet *applet,
                                                    const char *key,
                                                    gboolean value);
gboolean gnome_applet_config_get_boolean           (GnomeApplet *applet,
                                                    const char *key,
                                                    gboolean *value);
gboolean gnome_applet_config_get_boolean_default   (GnomeApplet *applet,
                                                    const char *key,
                                                    gboolean dvalue);
void gnome_applet_config_watch_boolean             (GnomeApplet *applet,
                                                    const char *key,
                                 GnomeAppletConfigBooleanChangeFunc callback,
                                                    gpointer user_data,
                                                    GDestroyNotify notify);


#endif /* _GNOME_APPLET_CONFIG_H_ */
