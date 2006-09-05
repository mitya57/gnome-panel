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

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus-glib-lowlevel.h>

#include <gtk/gtk.h>
#include <stdlib.h> /* strtol in menu item callback */
#include <string.h> /* strcmp in lookup_dbus_context */

#include "panel-profile.h"
#include "panel-enums.h"
#include "applet.h"

#include "gilded-star.h"

#include "gilded-star-dbus.h"


#ifndef FREEDESKTOP_BUG_7908_IS_FIXED

/* This awful hack mandated by the fact that dbus-glib doesn't know how
 * to marshal GParamSpec and doesn't provide a clean API to register our
 * own marshallers.  We mirror an internal data structure and set a
 * private property on the GParamSpec GType.
 *
 * This code should be removed after bug 7908 has been fixed.
 */

#include <dbus/dbus.h>

static gboolean
demarshal_gparamspec_value (void)
{
  /* no demarshalling.  we only ever send these as strings. */
  return FALSE;
}

static gboolean
marshal_gparamspec_value (DBusMessageIter *i, const GValue *value)
{
  GParamSpec *ps = value->data->v_pointer;

  dbus_message_iter_append_basic (i, DBUS_TYPE_STRING, &ps->name);

  return TRUE;
}

static void
register_dbus_gparamspec_marshaller (void)
{
  static const struct
  {
    const char *sig;
    const void *vtable_ptr;
    struct
    {
      void *marshaller;
      void *demarshaller;
    } vtable;
  } typedata = {
    "s",
    &typedata.vtable,
    {
      marshal_gparamspec_value,
      demarshal_gparamspec_value
    }
  };

  g_type_set_qdata (G_TYPE_PARAM,
                    g_quark_from_static_string ("DBusGTypeMetaData"),
                    (void *) &typedata);
}

#endif /* FREEDESKTOP_BUG_7908_IS_FIXED */

typedef struct _ContextAssociation ContextAssociation;
struct _ContextAssociation
{
  char *id;
  void *context;
  ContextAssociation *next;
};

static ContextAssociation *registry;

static void *
gilded_star_lookup_dbus_context (const char *id)
{
  ContextAssociation **node;

  for (node = &registry; *node; node = &(*node)->next)
    if (!strcmp ((*node)->id, id))
    {
      void *context = (*node)->context;
      ContextAssociation *tmp = *node;

      *node = (*node)->next;
      g_free (tmp->id);
      g_free (tmp);
      return context;
    }

  return NULL;
}

void
gilded_star_provide_dbus_context (const char *id, void *context)
{
  ContextAssociation *node;

  node = g_new (ContextAssociation, 1);

  node->id = g_strdup (id);
  node->context = context;
  node->next = registry;
  registry = node;
}

G_DEFINE_TYPE (GildedStar, gilded_star, GTK_TYPE_EVENT_BOX);

static void gilded_star_invoke (GildedStar *applet);

enum
{
  SIGNAL_MENUITEM,
  SIGNAL_CONFIG_CHANGED,
  NR_SIGNALS
};

static guint signals[NR_SIGNALS];

enum
{
  PROP_ORIENTATION = 1,
  PROP_BACKGROUND
};

typedef struct
{
  GildedStarBackgroundType background_type;
  GildedStarOrientation orientation;

  GtkWidget *drop_window;
  GtkWidget *drop_socket;
  GtkWidget *socket;
  GtkWidget *dialog;
  GtkWidget *image;

  AppletInfo *info;

  GildedStarLifetime life;
  char *icon;


  char *instance_name; /* on the dbus */
  char *id; /* like MyApplet/0 */
  char *type; /* like MyApplet */

  char *name; /* like My Cool Applet */

  GSList *gconf_notifies;

  gboolean seen_child;
  gboolean attached;

  GModule *module;
} GildedStarPrivate;

#define GET_PRIVATE(o) \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GILDED_STAR_TYPE, \
    GildedStarPrivate))

static gboolean
gilded_star_make_transparent (GtkWidget *widget)
{
  gdk_window_set_back_pixmap (widget->window, NULL, TRUE);

  return FALSE;
}

static gboolean
gilded_star_setup_transparency (GtkWidget *widget)
{
  if (GTK_WIDGET_APP_PAINTABLE (widget))
    return FALSE;

  gilded_star_make_transparent (widget);
  g_signal_connect_after (widget, "style_set",
                          G_CALLBACK (gilded_star_make_transparent), NULL);

  return FALSE;
}

static void
gilded_star_where_am_i (GildedStar *applet, int *x, int *y)
{
  GtkWidget *window;

  window = gtk_widget_get_toplevel (GTK_WIDGET (applet));
  gdk_window_get_origin (GTK_WIDGET (window)->window, x, y);
  *x += GTK_WIDGET (applet)->allocation.x;
  *y += GTK_WIDGET (applet)->allocation.y;
}

static GildedStar *dropdown; /* XXX name me! */

static void
gilded_star_toggle_drop_visibility (GildedStar *applet)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  int x, y;

  if (priv->drop_socket == NULL)
    return;

  if (dropdown == applet)
  {
    gtk_widget_hide (priv->drop_window);
    dropdown = NULL; 
    return;
  }

  if (dropdown != NULL)
    gilded_star_toggle_drop_visibility (dropdown);

  gilded_star_where_am_i (applet, &x, &y);
  y += GTK_WIDGET (applet)->allocation.height;
  gtk_window_move (GTK_WINDOW (priv->drop_window), x, y);

  gtk_widget_show (priv->drop_window);
  gtk_window_present (GTK_WINDOW (priv->drop_window));

  dropdown = applet;
}

static void
gilded_star_unregister (GildedStar *applet)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  GConfClient *client;
  const char *key;

  client = panel_gconf_get_client ();
  key = panel_gconf_sprintf (PANEL_CONFIG_DIR "/gilded_stars/%s", priv->id);
  gconf_client_recursive_unset (client, key, GCONF_UNSET_INCLUDING_SCHEMA_NAMES, NULL);
  panel_profile_remove_from_list (PANEL_GCONF_GILDED_STARS, priv->id);
}

static void
gilded_star_dialog_response (GildedStar *applet, gint response,
                              GtkDialog *dialog)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);

  gtk_widget_destroy (GTK_WIDGET (dialog));
  priv->dialog = NULL;

  switch (response)
  {
    case GTK_RESPONSE_YES:
      gilded_star_invoke (applet);
      break;

    case GTK_RESPONSE_NO:
      gilded_star_unregister (applet);
      break;
  }
}

static void
gilded_star_dialog (GildedStar *applet, const char *title,
                     const char *anon_title, const char *message,
                     const char *buttontext1, int response1,
                     const char *buttontext2, int response2)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  GtkWidget *dialog;

  if (priv->dialog)
    gtk_widget_destroy (priv->dialog);
  
  dialog = gtk_message_dialog_new (NULL, 0,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_NONE,
                                   priv->name ? title : anon_title,
                                   priv->name);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            message);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          buttontext1, response1,
                          buttontext2, response2,
                          NULL);

  g_signal_connect_object (dialog, "response",
                           G_CALLBACK (gilded_star_dialog_response),
                           applet, G_CONNECT_SWAPPED);

  priv->dialog = dialog;

  gtk_widget_show (dialog);
}

static void
gilded_star_unexpected_quit_dialog (GildedStar *applet)
{
  gilded_star_dialog (applet,
                       "\"%s\" has quit unexpectedly",
                       "An applet has quit unexpectedly",
                       "If you reload an applet it will automatically "
                       "be added back to the panel.",
                       "Do Not Reload", GTK_RESPONSE_NO,
                       "Reload",        GTK_RESPONSE_YES);
}

static void
gilded_star_failed_to_invoke (GildedStar *applet)
{
  gilded_star_dialog (applet,
                       "\"%s\" failed to start",
                       "An applet has failed to start",
                       "Do you want to delete the applet from "
                       "your configuration?",
                       "Don't Delete", GTK_RESPONSE_CANCEL,
                       "Delete",       GTK_RESPONSE_NO);
}

static gboolean
gilded_star_clicked_callback (GildedStar *applet, GdkEventButton *ev)
{
  if (ev->type != GDK_BUTTON_PRESS)
    return FALSE;

  switch (ev->button)
  {
    case 1:
      gilded_star_toggle_drop_visibility (applet);
      return TRUE;

    default:
      return FALSE;
  }
}

static void
gilded_star_mouse_enter (GildedStar *applet)
{
  if (dropdown != NULL && dropdown != applet)
    gilded_star_toggle_drop_visibility (applet);
}

static void
gilded_star_create_icon (GildedStar *star)
{
  GildedStarPrivate *priv = GET_PRIVATE (star);

  if (priv->icon)
  {
    GtkWidget *image;

    image = gtk_image_new_from_icon_name (priv->icon, GTK_ICON_SIZE_MENU);
    gtk_container_add (GTK_CONTAINER (star), image);
    gtk_widget_show_all (GTK_WIDGET (star));
    priv->image = image;
  }
  else
    gtk_widget_hide (GTK_WIDGET (star));
}

static gboolean
gilded_star_plug_removed (GildedStar *applet)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);

  priv->attached = FALSE;

  gtk_container_remove (GTK_CONTAINER (applet), priv->socket);
  priv->socket = NULL;

  gilded_star_create_icon (applet);

  /* we maybe got called because the entire applet is being
   * destroyed.  in that case, don't clean up our menu.
   */
  if (priv->info)
  {
    panel_applet_remove_callbacks (priv->info);

    if (priv->life == GILDED_STAR_LIFETIME_PERSISTENT)
      gilded_star_unexpected_quit_dialog (applet);
  }

  return TRUE;
}

static gboolean
gilded_star_drop_plug_removed (GildedStar *applet)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);

  gtk_widget_destroy (priv->drop_window);
  priv->drop_socket = NULL;
  priv->drop_window = NULL;

  return TRUE;
}

static void
gilded_star_init (GildedStar *applet)
{
  g_signal_connect_after (applet, "realize",
                          G_CALLBACK (gilded_star_setup_transparency), NULL);

  g_signal_connect (applet, "button-press-event",
                    G_CALLBACK (gilded_star_clicked_callback), NULL);
  g_signal_connect (applet, "enter-notify-event",
                    G_CALLBACK (gilded_star_mouse_enter), NULL);
}

static void
gilded_star_finalize (GildedStar *applet)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  GConfClient *client;
  GSList *item;

  g_print ("applet is being finalized\n");

  /* we will only be finalised if the panel disowns us.
   * this means we don't have to do anything with priv->info.
   */

  if (dropdown == applet)
    dropdown = NULL;

  if (priv->drop_socket)
    /* will take the socket with it since we don't own a ref */
    gtk_widget_destroy (priv->drop_window);

  client = panel_gconf_get_client ();

  for (item = priv->gconf_notifies; item; item = item->next)
    gconf_client_notify_remove (client, (guint) item->data);

  /* We can't close the dynamically loaded module (if it exists).
   * It (or the libraries it depends on) probably registered some static
   * types with GLib.  Unloading and reloading will cause all sorts of
   * bad things to happen therefore we do not unload.
   *
   * priv->module
   */

  g_free (priv->instance_name);
  g_free (priv->icon);
  g_free (priv->type);
  g_free (priv->name);
  g_free (priv->id);
}

static void
gilded_star_get_property (GObject *object, guint prop_id,
                     GValue *value, GParamSpec *pspec)
{
  GildedStarPrivate *priv = GET_PRIVATE (object);

  switch (prop_id)
  {
    case PROP_ORIENTATION:
      g_value_set_int (value, priv->orientation);
      break;

    case PROP_BACKGROUND:
      g_value_set_int (value, priv->background_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gilded_star_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  GildedStarPrivate *priv = GET_PRIVATE (widget);

  gboolean change = allocation->x != widget->allocation.x ||
                    allocation->y != widget->allocation.y ||
                    allocation->width != widget->allocation.width ||
                    allocation->height != widget->allocation.height;

  GTK_WIDGET_CLASS (gilded_star_parent_class)->size_allocate (widget, allocation);

  if (!change || priv->background_type == 0)
    return;

  if (priv->socket)
  {
    gtk_widget_hide (priv->socket);
    gtk_widget_show (priv->socket);
  }
}

static void
gilded_star_dispose (GObject *object)
{
  GildedStarPrivate *priv = GET_PRIVATE (object);

  if (priv->info)
  {
    //g_object_unref (priv->info);
    priv->info = NULL;
  }

  if (priv->dialog)
  {
    gtk_widget_destroy (priv->dialog);
    priv->dialog = NULL;
  }

  G_OBJECT_CLASS (gilded_star_parent_class)->dispose (object);
}
  
static void
gilded_star_class_init (GildedStarClass *class)
{
  DBusGConnection *dbus;
  GObjectClass *oc = G_OBJECT_CLASS (class);
  GtkWidgetClass *wc = GTK_WIDGET_CLASS (class);

  g_type_class_add_private (oc, sizeof (GildedStarPrivate));

  wc->size_allocate = gilded_star_size_allocate;

#ifndef FREEDESKTOP_BUG_7908_IS_FIXED
  register_dbus_gparamspec_marshaller ();
#endif

  dbus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

  dbus_g_object_type_install_info (GILDED_STAR_TYPE,
                                   &dbus_glib_gilded_star_object_info);

  G_OBJECT_CLASS (class)->finalize = (GObjectFinalizeFunc) gilded_star_finalize;

  oc->get_property = gilded_star_get_property;
  //oc->set_property = applet_set_property;
  oc->dispose = gilded_star_dispose;

  g_object_class_install_property (oc, PROP_ORIENTATION,
    g_param_spec_int("orientation", "Orientation",
                     "The orientation of the panel containing this applet",
                     0, 3, 0, G_PARAM_READABLE));

  g_object_class_install_property (oc, PROP_ORIENTATION,
    g_param_spec_int("background-type", "Background Type",
                     "The background type of the panel containing this applet",
                     0, 1, 0, G_PARAM_READABLE));

  /* C marshaller not needed since we never connect in-process */
  signals[SIGNAL_MENUITEM] =
    g_signal_new ("menu-item-selected",
                  G_OBJECT_CLASS_TYPE (class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, G_TYPE_INT);

  signals[SIGNAL_CONFIG_CHANGED] =
    g_signal_new ("config-changed",
                  G_OBJECT_CLASS_TYPE (class),
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_VALUE);
}

static GConfValue *
gilded_star_g_to_gconfvalue (const GValue *gvalue)
{
  GConfValue *gconfvalue;

  switch (G_VALUE_TYPE (gvalue))
  {
    case G_TYPE_INT: /* long and int64 won't fit! */
    case G_TYPE_UINT:
      gconfvalue = gconf_value_new (GCONF_VALUE_INT);
      gconf_value_set_int (gconfvalue, g_value_get_int (gvalue));
      break;

    case G_TYPE_STRING:
      gconfvalue = gconf_value_new (GCONF_VALUE_STRING);
      gconf_value_set_string (gconfvalue, g_value_get_string (gvalue));
      break;

    case G_TYPE_FLOAT:
    case G_TYPE_DOUBLE:
      gconfvalue = gconf_value_new (GCONF_VALUE_FLOAT);
      /* XXX if i get_double on a float gvalue will it auto-convert? */
      gconf_value_set_float (gconfvalue, g_value_get_double (gvalue));
      break;

    case G_TYPE_BOOLEAN:
      gconfvalue = gconf_value_new (GCONF_VALUE_BOOL);
      gconf_value_set_bool (gconfvalue, g_value_get_boolean (gvalue));
      break;

    default:
      gconfvalue = NULL;
      break;
  }
  return gconfvalue;
}

static gboolean
gilded_star_gconf_to_gvalue (GValue *gvalue, GConfValue *gconfvalue)
{
  if (gconfvalue == NULL)
    return FALSE;

  switch (gconfvalue->type)
  {
    case GCONF_VALUE_STRING:
      g_value_init (gvalue, G_TYPE_STRING);
      g_value_set_string (gvalue, gconf_value_get_string (gconfvalue));
      return TRUE;

    case GCONF_VALUE_INT:
      g_value_init (gvalue, G_TYPE_INT);
      g_value_set_int (gvalue, gconf_value_get_int (gconfvalue));
      return TRUE;

    case GCONF_VALUE_FLOAT:
      g_value_init (gvalue, G_TYPE_DOUBLE);
      g_value_set_double (gvalue, gconf_value_get_float (gconfvalue));
      return TRUE;

    case GCONF_VALUE_BOOL:
      g_value_init (gvalue, G_TYPE_BOOLEAN);
      g_value_set_boolean (gvalue, gconf_value_get_bool (gconfvalue));
      return TRUE;

    default:
      return FALSE;
  }
}

static void
gilded_star_gconf_prefs_notify (GConfClient *client, guint cnxn_id,
                                 GConfEntry *entry, gpointer user_data)
{
  GildedStar *applet =  (user_data);
  GValue value = { 0, };
  const char *key;

  if ((key = strstr (entry->key, "/prefs/")))
  {
    key += 7;
    if (gilded_star_gconf_to_gvalue (&value, entry->value))
    {
      g_signal_emit (applet, signals[SIGNAL_CONFIG_CHANGED], 0, key, &value);
      g_value_unset (&value);
    }
  }
}

static void
gilded_star_gconf_icon_notify (GConfClient *client, guint cnxn_id,
                               GConfEntry *entry, gpointer user_data)
{
  GildedStarPrivate *priv = GET_PRIVATE (user_data);

  g_free (priv->icon);

  if (entry->value && entry->value->type == GCONF_VALUE_STRING)
    priv->icon = g_strdup (gconf_value_get_string (entry->value));
  else
    priv->icon = NULL;
}

static void
gilded_star_gconf_name_notify (GConfClient *client, guint cnxn_id,
                                GConfEntry *entry, gpointer user_data)
{
  GildedStarPrivate *priv = GET_PRIVATE (user_data);

  g_free (priv->name);

  if (entry->value && entry->value->type == GCONF_VALUE_STRING)
    priv->name = g_strdup (gconf_value_get_string (entry->value));
  else
    priv->name = NULL;
}

static void
gilded_star_gconf_life_notify (GConfClient *client, guint gnxn_id,
                               GConfEntry *entry, gpointer user_data)
{
  GildedStarPrivate *priv = GET_PRIVATE (user_data);

  if (entry->value && entry->value->type == GCONF_VALUE_INT)
    priv->life = gconf_value_get_int (entry->value);
  else
    priv->life = GILDED_STAR_LIFETIME_NORMAL;
}

static void
gilded_star_create_socket (GildedStar *star)
{
  GildedStarPrivate *priv = GET_PRIVATE (star);

  if (priv->socket)
    return;

  if (priv->image)
    gtk_container_remove (GTK_CONTAINER (star), priv->image);
  priv->image = NULL;

  priv->socket = gtk_socket_new ();
  g_signal_connect_swapped (priv->socket, "plug-removed",
                            G_CALLBACK (gilded_star_plug_removed), star);
  g_signal_connect_after (priv->socket, "realize",
                          G_CALLBACK (gilded_star_setup_transparency), NULL);
  gtk_container_add (GTK_CONTAINER (star), priv->socket);
  gtk_widget_show_all (GTK_WIDGET (star));
}

static int
gilded_star_get_xid (GildedStar *applet)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);

  if (priv->attached)
    return 0;

  gilded_star_create_socket (applet);

  priv->seen_child = TRUE;
  priv->attached = TRUE;

  return gtk_socket_get_id (GTK_SOCKET (priv->socket));
}

static gboolean
gilded_star_give_xid (GildedStar *applet, int xid)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);

  if (priv->attached)
    return FALSE;

  gilded_star_create_socket (applet);

  priv->seen_child = TRUE;
  priv->attached = TRUE;

  gtk_socket_add_id (GTK_SOCKET (priv->socket), xid);
  return TRUE;
}

static int
fin (void)
{
  g_print ("fin\n");
  return FALSE;
}

static int
gilded_star_request_drop_socket (GildedStar *applet)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  GtkWidget *window, *socket, *frame;
  GtkWidget *container; /* workaround for bug #347937 */

  if (!priv->drop_socket)
  {
    container = gtk_vbox_new (0, 0);
    gtk_container_set_border_width (GTK_CONTAINER (container), 5);

    socket = gtk_socket_new ();

    frame = gtk_frame_new (NULL);
    gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
    gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_MENU);

    gtk_container_add (GTK_CONTAINER (window), frame);
    gtk_container_add (GTK_CONTAINER (frame), container);
    gtk_container_add (GTK_CONTAINER (container), socket);

    gtk_widget_show_all (frame);

    priv->drop_socket = socket;
    priv->drop_window = window;

    g_signal_connect_swapped (priv->drop_socket, "plug-removed",
                              G_CALLBACK (gilded_star_drop_plug_removed),
                              applet);
    g_signal_connect (priv->drop_socket, "destroy", G_CALLBACK (fin), NULL);
  }

  return gtk_socket_get_id (GTK_SOCKET (priv->drop_socket));
}

static int
gilded_star_add_menu_item (GildedStar *applet, const char *stock,
                            const char *label)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  static int unique_id;
  char number[32];

  // i'm so sorry that i've fallen
  // help me up, let's keep on running
  // don't let me fall out of love

  snprintf (number, 32, "%d", ++unique_id);
  panel_applet_add_callback (priv->info, number, stock, label, NULL);

  return unique_id;
}

static const char *
gilded_star_get_dbus_name (GildedStar *applet)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);

  return priv->instance_name;
}

static void
gilded_star_gconf_watch (GildedStar *applet, const char *name,
                         GConfClientNotifyFunc func)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  GConfClient *client;
  const char *key;
  int watch;

  client = panel_gconf_get_client ();
  key = panel_gconf_full_key (PANEL_GCONF_GILDED_STARS, priv->id, name);
  watch = gconf_client_notify_add (client, key, func, applet, NULL, NULL);
  priv->gconf_notifies = g_slist_prepend (priv->gconf_notifies,
                                          (gpointer) watch);
  gconf_client_notify (client, key);
}

static void
gilded_star_set_name_and_register (GildedStar *applet, const char *name)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  DBusGConnection *dbus;
  GConfClient *client;
  char *type;
  char *s;

  client = panel_gconf_get_client ();

  type = g_strdup (name);
  if ((s = strchr (type, '/')) != NULL)
  {
    char *schemas, *pref;

    *s = '\0';
    priv->type = g_strdup (type); /* XXX it's bad if this doesn't happen */
    pref = g_strdup_printf (PANEL_CONFIG_DIR "/gilded_stars/%s/prefs", name);
    schemas = g_strdup_printf ("/schemas/GildedStars/%s", type);
    panel_gconf_associate_schemas_in_dir (client, pref, schemas);
    g_free (schemas);
    g_free (pref);
    *s = '/';
  }

  priv->id = type;

  gilded_star_gconf_watch (applet, "prefs", gilded_star_gconf_prefs_notify);
  gilded_star_gconf_watch (applet, "icon", gilded_star_gconf_icon_notify);
  gilded_star_gconf_watch (applet, "name", gilded_star_gconf_name_notify);
  gilded_star_gconf_watch (applet, "life", gilded_star_gconf_life_notify);

  priv->instance_name = g_strdup_printf ("/ca/desrt/Applet/%s", name);
  dbus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
  dbus_g_connection_register_g_object (dbus, priv->instance_name,
                                       G_OBJECT (applet));
}

static GildedStarOrientation
gilded_star_get_orientation (GildedStar *applet)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  return priv->orientation;
}

static GildedStarBackgroundType
gilded_star_get_background_type (GildedStar *applet)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  return priv->background_type;
}

static void
gilded_star_set_orientation (GildedStar *applet, GildedStarOrientation orientation)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);

  if (priv->orientation == orientation)
    return;

  priv->orientation = orientation;

  g_object_notify (G_OBJECT (applet), "orientation");
}

static void
gilded_star_set_background_type (GildedStar *applet,
                                  GildedStarBackgroundType type)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);

  if ((priv->background_type || type) && priv->socket)
  {
    gtk_widget_hide (priv->socket);
    gtk_widget_show (priv->socket);
  }

  if (priv->background_type == type)
    return;

  priv->background_type = type;
  g_object_notify (G_OBJECT (applet), "background-type");
}

static void
gilded_star_set_info_pointer (GildedStar *applet, AppletInfo *info)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);

  priv->info = info;
  //g_object_ref (priv->info);
}

static void
gilded_star_gconf_set_string (GildedStar *applet, const char *key,
                               const char *string)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  GConfClient *client;
  const char *fullkey;

  client = panel_gconf_get_client ();
  fullkey = panel_gconf_full_key (PANEL_GCONF_GILDED_STARS, priv->id, key);

  if (string && string[0])
    gconf_client_set_string (client, fullkey, string, NULL);
  else
    gconf_client_unset (client, key, NULL);
}

static void
gilded_star_set_life (GildedStar *applet, GildedStarLifetime life)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  GConfClient *client;
  const char *fullkey;

  client = panel_gconf_get_client ();
  fullkey = panel_gconf_full_key (PANEL_GCONF_GILDED_STARS, priv->id, "life");
  gconf_client_set_int (client, fullkey, life, NULL);
}

static void
gilded_star_set_name (GildedStar *star, const char *name)
{
  gilded_star_gconf_set_string (star, "name", name);
}

static void
gilded_star_set_icon (GildedStar *star, const char *name)
{
  gilded_star_gconf_set_string (star, "icon", name);
}

static char *
gilded_star_get_path (GildedStar *applet, gboolean *library)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  GKeyFile *keyfile;
  char *filename;
  char *key;

  /* if any error happens here then we'll just get NULL */
  keyfile = g_key_file_new ();
  filename = g_strdup_printf ("applets/%s", priv->type);
  g_key_file_load_from_data_dirs (keyfile, filename, NULL, 0, NULL);
  g_free (filename);

  *library = FALSE;
  if ((key = g_key_file_get_string (keyfile, priv->type, "so", NULL)))
    *library = TRUE;
  else
    key = g_key_file_get_string (keyfile, priv->type, "path", NULL);
  g_key_file_free (keyfile);

  return key;
}


static void
gilded_star_child_exit (GPid pid, int status, gpointer data)
{
  GildedStar *applet =  (data);
  GildedStarPrivate *priv = GET_PRIVATE (applet);

  if (!priv->seen_child)
    gilded_star_failed_to_invoke (applet);
}

static void
gilded_star_invoke_binary (GildedStar *applet, const char *path)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  const char *argv[] = {path, priv->id, NULL};
  GPid pid;

  if (!g_spawn_async (NULL, (char **) argv, NULL,
                      G_SPAWN_DO_NOT_REAP_CHILD,
                      NULL, NULL, &pid, NULL))
  {
    gilded_star_child_exit (0, 0, applet);
    return;
  }

  g_child_watch_add_full (G_PRIORITY_DEFAULT, pid,
                          gilded_star_child_exit, applet,
                          g_object_unref);
  g_object_ref (applet);
}

static void
gilded_star_invoke_so (GildedStar *applet, const char *path)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  GtkWidget *(*initfunc) (const char *id);
  GtkWidget *widget;
  gpointer stupid;

  priv->module = g_module_open (path, G_MODULE_BIND_LOCAL);

  if (priv->module == NULL)
  {
    gilded_star_failed_to_invoke (applet);
    return;
  }

  g_module_symbol (priv->module, "weather_applet_new", &stupid);
  initfunc = stupid;

  if (initfunc == NULL)
  {
    gilded_star_failed_to_invoke (applet);
    return;
  }

  widget = initfunc (priv->id);

  if (widget == NULL)
  {
    gilded_star_failed_to_invoke (applet);
    return;
  }

  gtk_widget_show (widget);
  gilded_star_child_exit (0, 0, applet);
}

static void
gilded_star_invoke (GildedStar *applet)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  gboolean library;
  char *name;

  priv->seen_child = FALSE;

  if ((name = gilded_star_get_path (applet, &library)) == NULL)
  {
    gilded_star_failed_to_invoke (applet);
    return;
  }

  if (library)
    gilded_star_invoke_so (applet, name);
  else
    gilded_star_invoke_binary (applet, name);

  g_free (name);
}

static gboolean
gilded_star_config_get (GildedStar *applet, const char *name,
                        GValue *value, GError *error)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  GConfValue *gconfvalue;
  GConfClient *client;
  const char *key;
  gboolean result;

  client = panel_gconf_get_client ();
  key = panel_gconf_sprintf (PANEL_CONFIG_DIR "/gilded_stars/%s/prefs/%s",
                             priv->id, name);
  gconfvalue = gconf_client_get (client, key, NULL); /* XXX set error */

  if (gconfvalue == NULL) /* XXX set an error */
    return FALSE;

  result = gilded_star_gconf_to_gvalue (value, gconfvalue);
  gconf_value_free (gconfvalue);
/*XXX set error*/

  return result;
}

static gboolean
gilded_star_config_set (GildedStar *applet, const char *name,
                        const GValue *value, GError *error)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  GConfValue *gconfvalue;
  GConfClient *client;
  const char *key;

  gconfvalue = gilded_star_g_to_gconfvalue (value);

  if (gconfvalue == NULL) /* XXX set an error */
    return FALSE;

  client = panel_gconf_get_client ();
  key = panel_gconf_sprintf (PANEL_CONFIG_DIR "/gilded_stars/%s/prefs/%s",
                             priv->id, name);

  gconf_client_set (client, key, gconfvalue, NULL); /* XXX set error */
  gconf_value_free (gconfvalue);

  return TRUE;
}

static gboolean
gilded_star_config_touch (GildedStar *applet, const char *name)
{
  GildedStarPrivate *priv = GET_PRIVATE (applet);
  GConfClient *client;
  const char *key;

  client = panel_gconf_get_client ();
  key = panel_gconf_sprintf (PANEL_CONFIG_DIR "/gilded_stars/%s/prefs/%s",
                             priv->id, name);
  gconf_client_notify (client, key);

  return TRUE;
}

static void
gilded_star_setup (GildedStar *star)
{
  GildedStarPrivate *priv = GET_PRIVATE (star);

  switch (priv->life)
  {
    case GILDED_STAR_LIFETIME_PERSISTENT:
      gilded_star_invoke (star);
      break;

    case GILDED_STAR_LIFETIME_NORMAL:
      gilded_star_create_icon (star);
      break;

    default:
      break;
  }
}

/* -------------------------------------------------------------
    publicly accessible functions called from inside the panel
   ------------------------------------------------------------- */
void
gilded_star_load_from_gconf (PanelWidget *panel, gboolean locked,
                             int position, const char *id)
{
  GildedStarLifetime life = GILDED_STAR_LIFETIME_TEMPORARY;
  DBusGMethodInvocation *ctx;
  GConfClient *client;
  GildedStar *applet;
  AppletInfo *info;
  const char *key;

  client = panel_gconf_get_client ();

  /* The object is in one of the following three states:
   *
   * 1) A new applet just requested its creation.  In this case the
   *    context of the existing request will exist in the registry.  We
   *    can get it and return our ID to the newly created applet.
   *
   * 2) We are an old instance of a persistent applet.  In this case
   *    the "life" variable will be set to something other than
   *    GILDED_STAR_LIFETIME_TEMPORARY.
   *
   * 3) A temporary applet wasn't cleaned up from last run.  In this
   *    case, remove ourselves from the profile.
   */

  /* 1: Check for a valid dbus_context_id */
  ctx = gilded_star_lookup_dbus_context (id); /* or NULL */

  /* ctx is set iff we found our pending applet creation request */

  /* 2: Check for a specified executable */
  if (!ctx)
  {
    key = panel_gconf_full_key (PANEL_GCONF_GILDED_STARS, id, "life");
    life = gconf_client_get_int (client, key, NULL);
  }

  /* 3: If we have no context and the applet was temporary then quit.
   */
  if (!ctx && life == GILDED_STAR_LIFETIME_TEMPORARY)
  {
    g_print ("'%s' found to be DOA.  Bye.\n", id);
    panel_profile_remove_from_list (PANEL_GCONF_GILDED_STARS, id);
    return;
  }

  /* If we are here, we will create the applet now. */
  applet = g_object_new (GILDED_STAR_TYPE, NULL);

  info = panel_applet_register (GTK_WIDGET (applet), NULL, NULL,
                                panel, locked, position,
                                TRUE, PANEL_OBJECT_GILDED_STAR, id);
  gtk_widget_hide (GTK_WIDGET (applet));

  gilded_star_set_info_pointer (applet, info);
  panel_widget_set_applet_expandable (panel, GTK_WIDGET (applet), FALSE, TRUE);
  panel_widget_set_applet_size_constrained (panel, GTK_WIDGET (applet), TRUE);

  gilded_star_set_name_and_register (applet, id);

  if (ctx)
    dbus_g_method_return (ctx, g_strdup (gilded_star_get_dbus_name (applet)));
  else
    gilded_star_setup (applet);
}

void
gilded_star_menu_item_callback (GildedStar *applet, const char *item)
{
  long item_nr;
  char *end;

  item_nr = strtol (item, &end, 10);

  if (item_nr < 0 || item_nr > 1000 || *end)
    return;

  g_signal_emit (applet, signals[SIGNAL_MENUITEM], 0, item_nr);
}

void
gilded_star_set_panel_orientation (GildedStar *applet, int p)
{
  switch (p)
  {
/* brain damage... */
    case PANEL_ORIENTATION_TOP:
      gilded_star_set_orientation (applet, GILDED_STAR_ORIENTATION_TOP);
      break;

    case PANEL_ORIENTATION_BOTTOM:
      gilded_star_set_orientation (applet, GILDED_STAR_ORIENTATION_BOTTOM);
      break;

    case PANEL_ORIENTATION_LEFT:
      gilded_star_set_orientation (applet, GILDED_STAR_ORIENTATION_LEFT);
      break;

    case PANEL_ORIENTATION_RIGHT:
      gilded_star_set_orientation (applet, GILDED_STAR_ORIENTATION_RIGHT);
      break;
  }
}

void
gilded_star_set_panel_background_type (GildedStar *applet,
                                        PanelBackgroundType type)
{
  gilded_star_set_background_type (applet, type ? 1 : 0);
}
