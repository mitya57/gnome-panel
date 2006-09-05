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

#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <gdk/gdkx.h>

#include "applet-server-client.h"
#include "applet-client.h"

#include "gnome-applet-marshal.h"

#include "gnome-applet-config.h" /* XXX ew */

#include "gnome-applet-background-type.h"
#include "gnome-applet-orientation.h"
#include "gnome-applet-lifespan.h"
#include "gnome-applet-label.h"
#include "gnome-applet-layout-box.h"

#include "gnome-applet.h"

#define HANDLE_SIZE 10

static GtkWidgetClass *parent_class = NULL;

typedef struct _MenuItem MenuItem;

struct _MenuItem
{
  int id;
  void (*callback)(GnomeApplet *applet, gpointer user_data);
  gpointer user_data;
  GDestroyNotify unref;

  MenuItem *next;
};

typedef struct _ConfigWatch ConfigWatch;

struct _ConfigWatch
{
  char *key;
  GnomeAppletConfigMarshalFunc marshaller;
  GnomeAppletConfigChangeFunc callback;
  gpointer user_data;
  GDestroyNotify notify;

  ConfigWatch *next;
};

struct _GnomeAppletPrivate
{
  DBusGProxy *proxy;
  GtkContainer *drop_plug;

  GnomeAppletBackgroundType background_type;
  GnomeAppletOrientation orientation;

  GnomeAppletLayoutBox *box;
  MenuItem *menu_items;
  ConfigWatch *config_watches;

  gboolean has_handle;
  char *id;

  int expose_skip;
};

#define GET_PRIVATE(o) \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOME_TYPE_APPLET, \
    GnomeAppletPrivate))

enum
{
  PROP_ID = 1,
  PROP_ORIENTATION,
  PROP_BACKGROUND,
  PROP_HANDLE
};

static GdkPixmap *
gnome_applet_get_background_pixmap (GtkWidget *widget)
{
  GdkPixmap *pixmap;
  int width, height;
  Window window;
  Drawable pix;
  Display *dpy;
  GdkGC *gc;
  GC xgc;

/* We have to XClearArea the window which will cause the window to be blank
 * until the next expose event finishes.  We can save and restore the
 * previous contents quickly so that they'll stay there until the expose
 * event finishes.  Some extra code and it may not produce that much of an
 * effect.
 */
#define POSSIBLY_LESS_FLICKER

#ifdef POSSIBLY_LESS_FLICKER
  GdkPixmap *temp;
  Drawable tmp;
#endif

  dpy = GDK_WINDOW_XDISPLAY (widget->window);
  window = GDK_WINDOW_XID (widget->window);

  width = widget->allocation.width;
  height = widget->allocation.height;
  
#ifdef POSSIBLY_LESS_FLICKER
  temp = gdk_pixmap_new (GDK_DRAWABLE (widget->window), width, height, -1);
  tmp = GDK_PIXMAP_XID (temp);
#endif

  pixmap = gdk_pixmap_new (GDK_DRAWABLE (widget->window), width, height, -1);
  gc = gdk_gc_new (GDK_DRAWABLE (pixmap));
  pix = GDK_PIXMAP_XID (pixmap);
  xgc = GDK_GC_XGC (gc);

  XSetWindowBackgroundPixmap (dpy, window, ParentRelative);
#ifdef POSSIBLY_LESS_FLICKER
  XCopyArea (dpy, window, tmp, xgc, 0, 0, width, height, 0, 0);
#endif
  XClearWindow (dpy, window);
  XCopyArea (dpy, window, pix, xgc, 0, 0, width, height, 0, 0);
#ifdef POSSIBLY_LESS_FLICKER
  XCopyArea (dpy, tmp, window, xgc, 0, 0, width, height, 0, 0);
  gdk_pixmap_unref (temp);
#endif

  gdk_gc_destroy (gc);
  return pixmap;
}

static void
gnome_applet_update_widget_pixmap (GtkWidget *widget)
{
  GdkPixmap *pixmap;
  GtkStyle *style;

  pixmap = gnome_applet_get_background_pixmap (widget);

  style = gtk_style_copy (widget->style);
  if (style->bg_pixmap[GTK_STATE_NORMAL])
    g_object_unref (style->bg_pixmap[GTK_STATE_NORMAL]);
  style->bg_pixmap[GTK_STATE_NORMAL] = g_object_ref (pixmap);
  gtk_widget_set_style (widget, style);
  g_object_unref (style);
}
 
static gboolean
gnome_applet_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (widget);

  if (event->area.x == 0 && event->area.y == 0 &&
      event->area.width == widget->allocation.width &&
      event->area.height == widget->allocation.height &&
      !priv->expose_skip--)
  {
    gnome_applet_update_widget_pixmap (widget);

    /* we just changed the style so another expose event is coming.
     * we make sure that this code doesn't run on the next expose and
     * we quit this current expose event now and let the next one
     * take care of everything.
     */
    priv->expose_skip = 1;
    return TRUE;
  }

  /* the upstream expose event handler will draw the children for us */
  parent_class->expose_event (widget, event);

  /* draw our handle if we have one */
  if (priv->has_handle)
  {
    GtkOrientation orientation;
    int x, y, width, height;

    y = 0;

    if (gnome_applet_orientation_is_horizontal (priv->orientation))
    {
      if (widget->allocation.width < HANDLE_SIZE)
        return FALSE;

      orientation = GTK_ORIENTATION_HORIZONTAL;
      height = widget->allocation.height;
      width = HANDLE_SIZE;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
        x = 0;
      else
        x = widget->allocation.width - HANDLE_SIZE;
    }
    else
    {
      if (widget->allocation.height < HANDLE_SIZE)
        return FALSE;

      orientation = GTK_ORIENTATION_VERTICAL;
      width = widget->allocation.width;
      height = HANDLE_SIZE;
      x = 0;
    }

    gtk_paint_handle (widget->style, widget->window,
                      GTK_WIDGET_STATE (widget), GTK_SHADOW_OUT,
                      &event->area, widget, "handlebox",
                      x, y, width, height, orientation);
  }

  return FALSE;
}

static void
gnome_applet_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  int border = GTK_CONTAINER (widget)->border_width;
  GnomeAppletPrivate *priv = GET_PRIVATE (widget);
  GtkBin *bin = GTK_BIN (widget);
  GtkAllocation me;
  GtkWidget *child;

  /* if any of the following are true, just do this normally:
   *   - we are not displaying a handle
   *   - we do not have room for a handle
   *   - we don't (yet) have a child
   */
  if (!priv->has_handle || allocation->width < HANDLE_SIZE || !bin->child)
  {
    parent_class->size_allocate (widget, allocation);
    return;
  }

  /* else, we're drawing a handle -- this gets fancy */
  me = *allocation;

  /* temporarily set the child to NULL while calling upstream size_allocate
   * to prevent it from size_allocating the child (which we want to do).
   */
  child = bin->child;
  bin->child = NULL;
  parent_class->size_allocate (widget, allocation);
  bin->child = child;

  /* make room for the handle and the border */
  me.x = border;
  me.y = border;
  me.width -= border * 2;
  me.height -= border * 2;

  if (gnome_applet_orientation_is_horizontal (priv->orientation))
  {
    me.width -= HANDLE_SIZE;
    if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
      me.x += HANDLE_SIZE;
  }
  else
  {
    me.height -= HANDLE_SIZE;
    me.y += HANDLE_SIZE;
  }

  /* ensure at least 1x1 for the child */
  if (me.width < 1)
    me.width = 1;

  if (me.height < 1)
    me.height = 1;

  /* size_allocate the child */
  gtk_widget_size_allocate (child, &me);
}

static void
gnome_applet_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (widget);

  /* do a normal size request */
  parent_class->size_request (widget, requisition);

  /* then add room for a handle if we want one */
  if (priv->has_handle)
  { /* <-- placate gcc */
    if (gnome_applet_orientation_is_horizontal (priv->orientation))
      requisition->width += HANDLE_SIZE;
    else
      requisition->height += HANDLE_SIZE;
  }
}

static DBusGProxy *
gnome_applet_devine_new_proxy (const char *id, const char *type)
{
  static DBusGProxy *server;
  DBusGProxy *applet_proxy;
  DBusGConnection *dbus;
  GError *error = NULL;
  char *object;

  dbus = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

  if (server == NULL)
    server = dbus_g_proxy_new_for_name (dbus, "ca.desrt.AppletServer",
                                             "/ca/desrt/AppletServer",
                                              "ca.desrt.AppletServer");

  if (server == NULL)
    return NULL;

  if (id)
    object = g_strdup_printf ("/ca/desrt/Applet/%s", id);
  else
    if (!ca_desrt_AppletServer_create_applet (server, type, &object, &error))
    {
      g_critical ("Unable to create remote applet instance: %s.  "
                  "Are you sure the panel is running?", error->message);
      g_clear_error (&error);

      return NULL;
    }

  applet_proxy = dbus_g_proxy_new_for_name (dbus, "ca.desrt.AppletServer",
                                            object, "ca.desrt.Applet");

  g_free (object);

  return applet_proxy;
}

static void
gnome_applet_get_property (GObject *object, guint prop_id,
                           GValue *value, GParamSpec *pspec)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (object);

  switch (prop_id)
  {
    case PROP_ORIENTATION:
      g_value_set_enum (value, priv->orientation);
      break;

    case PROP_BACKGROUND:
      g_value_set_enum (value, priv->background_type);
      break;

    case PROP_HANDLE:
      g_value_set_boolean (value, priv->has_handle);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gnome_applet_set_property (GObject *object, guint prop_id,
                           const GValue *value, GParamSpec *pspec)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (object);
  GnomeApplet *applet = GNOME_APPLET (object);

  switch (prop_id)
  {
    case PROP_ID:
      priv->id = g_value_dup_string (value);
      break;
      
    case PROP_HANDLE:
      gnome_applet_set_has_handle (applet, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gnome_applet_background_notify (GtkWidget *widget, gpointer type)
{
  if (GNOME_IS_APPLET_LABEL (widget))
    gnome_applet_label_set_transparency_flag (GNOME_APPLET_LABEL (widget),
                                              GPOINTER_TO_INT (type));

  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
                           gnome_applet_background_notify,
                           type);
}

static void
gnome_applet_background_changed (GnomeApplet *applet)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (applet);
  GnomeAppletBackgroundType type;
  GtkBin *bin = GTK_BIN (applet);
  int temp;

  ca_desrt_Applet_get_background_type (priv->proxy, &temp, NULL);
  type = temp;

  if (priv->background_type == type)
    return;
  
  priv->background_type = type;

  if (bin->child)
    gnome_applet_background_notify (bin->child, GINT_TO_POINTER (type));

  //gtk_widget_queue_draw (GTK_WIDGET (applet));

  g_object_notify (G_OBJECT (applet), "background-type");
}

static void
gnome_applet_orientation_changed (GnomeApplet *applet)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (applet);
  GnomeAppletOrientation orientation;
  GtkBin *bin = GTK_BIN (applet);
  int temp;

  ca_desrt_Applet_get_orientation (priv->proxy, &temp, NULL);
  orientation = temp;

  if (priv->orientation == orientation)
    return;
  
  priv->orientation = orientation;

  if (GNOME_IS_APPLET_LAYOUT_BOX (bin->child))
    gnome_applet_layout_box_set_orientation (GNOME_APPLET_LAYOUT_BOX (bin->child), orientation);

  g_object_notify (G_OBJECT (applet), "orientation");
}

static void
gnome_applet_notify (DBusGProxy *proxy, const char *property,
                     GnomeApplet *applet)
{
  if (!strcmp (property, "orientation"))
    gnome_applet_orientation_changed (applet);
  else if (!strcmp (property, "background-type"))
    gnome_applet_background_changed (applet);
}

static void
gnome_applet_config_changed (DBusGProxy *proxy, const char *key,
                             const GValue *value, GnomeApplet *applet)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (applet);
  ConfigWatch *watch;

  for (watch = priv->config_watches; watch; watch = watch->next)
    if (!strcmp (watch->key, key))
    { /* <-- placate gcc */
      if (watch->marshaller)
        watch->marshaller (applet, key, value, watch->callback,
                           watch->user_data);
      else
        watch->callback (applet, key, value, watch->user_data);
    }
}

static void
gnome_applet_menu_item_selected (DBusGProxy *proxy, int item_id,
                                 GnomeApplet *applet)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (applet);
  MenuItem *item;

  for (item = priv->menu_items; item; item = item->next)
    if (item->id == item_id)
    {
      item->callback (applet, item->user_data);
      return;
    }

  g_warning ("panel sent unregistered menu item id %d", item_id);
}

static void
gnome_applet_initialise (GnomeApplet *applet, GType type)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (applet);
  const char *tname;
  int xid;

  tname = g_type_name (type);

  if (!(priv->proxy = gnome_applet_devine_new_proxy (priv->id, tname)))
    return;

  xid = gtk_plug_get_id (GTK_PLUG (applet));
  if (!ca_desrt_Applet_give_xid (priv->proxy, xid, NULL))
  //if (!ca_desrt_Applet_get_xid (priv->proxy, &xid, NULL))
  {
    g_critical ("Unable to connect to applet instance %s\n", priv->id);
    g_object_unref (priv->proxy);
    priv->proxy = NULL;
    return;
  }

  dbus_g_proxy_add_signal (priv->proxy, "Notify",
                           G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (priv->proxy, "Notify",
                               G_CALLBACK (gnome_applet_notify),
                               applet, NULL);

  dbus_g_object_register_marshaller (gnome_applet_marshal_VOID__POINTER_POINTER,
                                     G_TYPE_NONE,
                                     G_TYPE_STRING,
                                     G_TYPE_VALUE,
                                     G_TYPE_INVALID);
  dbus_g_proxy_add_signal (priv->proxy, "ConfigChanged",
                           G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (priv->proxy, "ConfigChanged",
                               G_CALLBACK (gnome_applet_config_changed),
                               applet, NULL);

  dbus_g_proxy_add_signal (priv->proxy, "MenuItemSelected",
                           G_TYPE_INT, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal (priv->proxy, "MenuItemSelected",
                               G_CALLBACK (gnome_applet_menu_item_selected),
                               applet, NULL);

  gnome_applet_orientation_changed (applet);
  gnome_applet_background_changed (applet);
  //gtk_plug_construct (GTK_PLUG (applet), xid);
}

static GObject *
gnome_applet_constructor (GType type, guint n_properties,
                          GObjectConstructParam *props)
{
  GObjectClass *pc;
  GObject *obj;

  pc = G_OBJECT_CLASS (parent_class);
  obj = pc->constructor (type, n_properties, props);

  gnome_applet_initialise (GNOME_APPLET (obj), type);

  return obj;
}

static void
gnome_applet_class_init (GnomeAppletClass *class)
{
  GObjectClass *oc;
  GtkWidgetClass *wc;

  parent_class = g_type_class_peek_parent (class);

  wc = GTK_WIDGET_CLASS (class);
  oc = G_OBJECT_CLASS (class);

  g_type_class_add_private (class, sizeof (GnomeAppletPrivate));

  wc->size_allocate = gnome_applet_size_allocate;
  wc->size_request = gnome_applet_size_request;
  wc->expose_event = gnome_applet_expose_event;

  oc->get_property = gnome_applet_get_property;
  oc->set_property = gnome_applet_set_property;
  oc->constructor = gnome_applet_constructor;

  g_object_class_install_property (oc, PROP_ID,
    g_param_spec_string ("applet-id", "Applet ID",
                         "The ID of this applet within the panel",
                         "def", G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (oc, PROP_ORIENTATION,
    g_param_spec_enum ("orientation", "Orientation",
                       "The orientation of the panel containing this applet",
                       GNOME_TYPE_APPLET_ORIENTATION,
                       GNOME_APPLET_ORIENTATION_TOP,
                       G_PARAM_READABLE));

  g_object_class_install_property (oc, PROP_BACKGROUND,
    g_param_spec_enum ("background-type", "Background Type",
                       "The background type of panel containing this applet",
                       GNOME_TYPE_APPLET_BACKGROUND_TYPE,
                       GNOME_APPLET_BACKGROUND_TYPE_PLAIN,
                       G_PARAM_READABLE));

  g_object_class_install_property (oc, PROP_HANDLE,
    g_param_spec_boolean ("has-handle", "Has Handle",
                          "If the applet should be drawn with a handle",
                          FALSE, G_PARAM_READWRITE));
}


GnomeApplet *
gnome_applet_new (void)
{
  return g_object_new (GNOME_TYPE_APPLET, NULL);
}

GtkContainer *
gnome_applet_get_drop_container (GnomeApplet *ga)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (ga);

  if (priv->drop_plug == NULL)
  {
    GtkWidget *plug;
    int xid;

    ca_desrt_Applet_request_drop_socket (priv->proxy, &xid, NULL);

    plug = gtk_plug_new (xid);
    gtk_widget_show (plug);
    priv->drop_plug = GTK_CONTAINER (plug);
  }

  return priv->drop_plug;
}

void
gnome_applet_add_dropdown_widget (GnomeApplet *ga, GtkWidget *widget)
{
  GtkContainer *drop_container;

  drop_container = gnome_applet_get_drop_container (ga);

  gtk_container_add (drop_container, widget);
  gtk_widget_show (widget);
}

static void
gnome_applet_create_layout_box (GnomeApplet *ga)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (ga);

  if (priv->box)
    return;

  if (GTK_BIN(ga)->child != NULL)
  {
    g_warning ("Attempting to use set_image or set_text functions on "
               "an applet that already has a child.  You may only use "
               "these functions if not providing your own child.");
    return;
  }

  priv->box = gnome_applet_layout_box_new ();
  gnome_applet_layout_box_set_orientation (priv->box, priv->orientation);
  gtk_widget_show_all (GTK_WIDGET (priv->box));
  gtk_container_add (GTK_CONTAINER (ga), GTK_WIDGET (priv->box));
}

static GtkImage *
gnome_applet_layout_box_image (GnomeApplet *ga)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (ga);

  gnome_applet_create_layout_box (ga);
  return gnome_applet_layout_box_request_image (priv->box);
}

static GnomeAppletLabel *
gnome_applet_layout_box_label (GnomeApplet *ga)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (ga);

  gnome_applet_create_layout_box (ga);
  return gnome_applet_layout_box_request_label (priv->box);
}

void
gnome_applet_set_image_from_file (GnomeApplet *ga, const char *filename)
{
  GtkImage *image = gnome_applet_layout_box_image (ga);
  gtk_image_set_from_file (image, filename);
}

void
gnome_applet_set_image_from_pixmap (GnomeApplet *ga, GdkPixmap *pixmap,
                                    GdkBitmap *mask)
{
  GtkImage *image = gnome_applet_layout_box_image (ga);
  gtk_image_set_from_pixmap (image, pixmap, mask);
}

void
gnome_applet_set_image_from_pixbuf (GnomeApplet *ga, GdkPixbuf *pixbuf)
{
  GtkImage *image = gnome_applet_layout_box_image (ga);
  gtk_image_set_from_pixbuf (image, pixbuf);
}

void
gnome_applet_set_image_from_stock (GnomeApplet *ga, const char *id)
{
  GtkImage *image = gnome_applet_layout_box_image (ga);
  gtk_image_set_from_stock (image, id, GTK_ICON_SIZE_MENU);
}

void
gnome_applet_set_image_from_icon_name (GnomeApplet *ga, const char *id)
{
  GtkImage *image = gnome_applet_layout_box_image (ga);
  gtk_image_set_from_icon_name (image, id, GTK_ICON_SIZE_MENU);
}

void
gnome_applet_set_text (GnomeApplet *ga, const char *text)
{
  GnomeAppletLabel *label = gnome_applet_layout_box_label (ga);
  gnome_applet_label_set_text (label, text);
}

void
gnome_applet_setf_text (GnomeApplet *ga, const char *fmt, ...)
{
  va_list ap;
  char *text;

  va_start (ap, fmt);
  text = g_strdup_vprintf (fmt, ap);
  gnome_applet_set_text (ga, text);
  g_free (text);
}

GnomeAppletBackgroundType
gnome_applet_get_background_type (GnomeApplet *ga)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (ga);

  return priv->background_type;
}

void
gnome_applet_set_has_handle (GnomeApplet *ga, gboolean handle)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (ga);

  if (priv->has_handle == handle)
    return;

  priv->has_handle = handle;

  gtk_widget_queue_resize (GTK_WIDGET (ga));

  g_object_notify (G_OBJECT (ga), "has-handle");
}

gboolean
gnome_applet_get_has_handle (GnomeApplet *ga)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (ga);

  return priv->has_handle;
}

GType
gnome_applet_get_type (void)
{
  static GType type_id;
  
  if (!type_id)
  {
    static const GTypeInfo typeinfo =
    {
      .instance_size = sizeof (GnomeApplet),
      .class_size = sizeof (GnomeAppletClass),
      .class_init = (GClassInitFunc) gnome_applet_class_init,
    };
    
    type_id = g_type_register_static (GTK_TYPE_PLUG, "GnomeApplet",
                                      &typeinfo, (GTypeFlags) 0);
  }

  return type_id;
}

void
gnome_applet_set_life (GnomeApplet *applet, GnomeAppletLifespan life)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (applet);

  ca_desrt_Applet_set_life (priv->proxy, life, NULL);
}

void
gnome_applet_set_name (GnomeApplet *applet, const char *name)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (applet);

  ca_desrt_Applet_set_name (priv->proxy, name, NULL);
}

void
gnome_applet_add_menu_item (GnomeApplet *applet, const char *stock,
                            const char *text, gpointer callback,
                            gpointer user_data, GDestroyNotify unref)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (applet);
  MenuItem *item;
  int id;

  if (!ca_desrt_Applet_add_menu_item (priv->proxy, stock, text, &id, NULL))
    return;

  item = g_new0 (MenuItem, 1);
  item->callback = callback;
  item->user_data = user_data;
  item->unref = unref;
  item->id = id;
  item->next = priv->menu_items;
  priv->menu_items = item;
}

gboolean
gnome_applet_config_get (GnomeApplet *applet, const char *key, GValue *value)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (applet);

  return ca_desrt_Applet_config_get (priv->proxy, key, value, NULL);
}

gboolean
gnome_applet_config_set (GnomeApplet *applet, const char *key, const GValue *value)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (applet);

  return ca_desrt_Applet_config_set (priv->proxy, key, value, NULL);
}

void
gnome_applet_config_watch_with_marshaller (GnomeApplet *applet,
                                           const char *key,
                                           GnomeAppletConfigMarshalFunc mrshl,
                                           gpointer callback,
                                           gpointer user_data,
                                           GDestroyNotify notify)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (applet);
  ConfigWatch *watch;

  watch = g_new0 (ConfigWatch, 1);
  watch->key = g_strdup (key);
  watch->marshaller = mrshl;
  watch->callback = callback;
  watch->user_data = user_data;
  watch->notify = notify;
  watch->next = priv->config_watches;
  priv->config_watches = watch;

  gnome_applet_config_touch (applet, key);
}

void
gnome_applet_config_touch (GnomeApplet *applet, const char *key)
{
  GnomeAppletPrivate *priv = GET_PRIVATE (applet);

  ca_desrt_Applet_config_touch (priv->proxy, key, NULL);
}
