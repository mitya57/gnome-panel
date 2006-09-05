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

#include "gnome-applet.h"

#include "gnome-applet-label.h"

#define DROP_SHADOW_SHIFT   1

G_DEFINE_TYPE (GnomeAppletLabel, gnome_applet_label, GTK_TYPE_CONTAINER);

enum
{
  PROP_TEXT = 1
};

struct _GnomeAppletLabelPrivate
{
  GtkWidget *white, *black;
  int background_type;
  char *text;

  gboolean sideways;
  int length, thickness;
};

#define GET_PRIVATE(o) \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNOME_TYPE_APPLET_LABEL, \
    GnomeAppletLabelPrivate))

static void
gnome_applet_label_init (GnomeAppletLabel *gal)
{
  GnomeAppletLabelPrivate *priv = GET_PRIVATE (gal);
  GdkColor black;

  gdk_color_parse ("black", &black);

  priv->white = gtk_label_new ("");
  priv->black = gtk_label_new ("");

  GTK_WIDGET_SET_FLAGS (gal, GTK_NO_WINDOW);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (gal), FALSE);

  gtk_widget_modify_fg (priv->black, GTK_STATE_NORMAL, &black);

  gtk_widget_set_parent (priv->white, GTK_WIDGET (gal));
  gtk_widget_set_parent (priv->black, GTK_WIDGET (gal));
  gtk_widget_show (priv->white);
  gtk_widget_hide (priv->black);
}

static void
gnome_applet_label_size_request (GtkWidget *widget, GtkRequisition *req)
{
  GnomeAppletLabelPrivate *priv = GET_PRIVATE (widget);

  gtk_widget_size_request (priv->white, req);
  gtk_widget_size_request (priv->black, req);
  req->width += DROP_SHADOW_SHIFT;
  req->height += DROP_SHADOW_SHIFT;
  widget->requisition = *req;

  if (priv->sideways)
    priv->thickness = req->width, priv->length = req->height;
  else
    priv->thickness = req->height, priv->length = req->width;
}

static void
gnome_applet_label_size_allocate (GtkWidget *widget, GtkAllocation *alloc)
{
  GnomeAppletLabelPrivate *priv = GET_PRIVATE (widget);
  GtkAllocation allocation;

  widget->allocation = *alloc;
  allocation = *alloc;

  allocation.height -= DROP_SHADOW_SHIFT;
  allocation.width -= DROP_SHADOW_SHIFT;

  gtk_widget_size_allocate (priv->white, &allocation);

  allocation = *alloc;
  allocation.height -= DROP_SHADOW_SHIFT;
  allocation.width -= DROP_SHADOW_SHIFT;
  allocation.x += DROP_SHADOW_SHIFT;
  allocation.y += DROP_SHADOW_SHIFT;
  gtk_widget_size_allocate (priv->black, &allocation);
}

static void
gnome_applet_label_forall (GtkContainer *container, gboolean internals,
                           GtkCallback callback, gpointer user_data)
{
  GnomeAppletLabelPrivate *priv = GET_PRIVATE (container);

  if (!internals)
    return;

  (*callback) (priv->black, user_data);
  (*callback) (priv->white, user_data);
}

void
gnome_applet_label_set_transparency_flag (GnomeAppletLabel *gal, int type)
{
  GnomeAppletLabelPrivate *priv = GET_PRIVATE (gal);

  if (priv->background_type == type)
    return;

  priv->background_type = type;
  
  if (type != GNOME_APPLET_BACKGROUND_TYPE_PLAIN)
  {
    GdkColor white;

    gdk_color_parse ("white", &white);
    gtk_widget_modify_fg (priv->white, GTK_STATE_NORMAL, &white);
    gtk_widget_show (priv->black);
  }
  else
  {
    /* blow away the modified style (unwhite the label) */
    GtkRcStyle *rc = gtk_rc_style_new ();
    gtk_widget_modify_style (priv->white, rc);
    gtk_rc_style_unref (rc);

    gtk_widget_hide (priv->black);
  }
}

static void
gnome_applet_label_map (GtkWidget *widget)
{
  GnomeAppletLabel *gal = GNOME_APPLET_LABEL (widget);
  GnomeAppletLabelPrivate *priv = GET_PRIVATE (gal);
  GtkWidget *applet;
  int type = 0;
  
  applet = gtk_widget_get_toplevel (widget);
  if (GNOME_IS_APPLET (applet))
    type = gnome_applet_get_background_type (GNOME_APPLET (applet));
  gnome_applet_label_set_transparency_flag (gal, type);

  GTK_WIDGET_CLASS (gnome_applet_label_parent_class)->map (widget);
}

static GType
gnome_applet_label_child_type (GtkContainer *container)
{
  return G_TYPE_NONE;
}

static void
gnome_applet_label_get_property (GObject *object, guint prop_id,
                                 GValue *value, GParamSpec *pspec)
{
  GnomeAppletLabel *gal = GNOME_APPLET_LABEL (object);
  GnomeAppletLabelPrivate *priv = GET_PRIVATE (gal);

  switch (prop_id)
  {
    case PROP_TEXT:
      g_value_set_string (value, priv->text);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gnome_applet_label_set_property (GObject *object, guint prop_id,
                                 const GValue *value, GParamSpec *pspec)
{
  GnomeAppletLabel *gal = GNOME_APPLET_LABEL (object);

  switch (prop_id)
  {
    case PROP_TEXT:
      gnome_applet_label_set_text (gal, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gnome_applet_label_class_init (GnomeAppletLabelClass *class)
{
  GtkContainerClass *cc;
  GtkWidgetClass *wc;
  GObjectClass *oc;

  g_type_class_add_private (class, sizeof (GnomeAppletLabelPrivate));

  cc = GTK_CONTAINER_CLASS (class);
  wc = GTK_WIDGET_CLASS (class);
  oc = G_OBJECT_CLASS (class);

  wc->size_request = gnome_applet_label_size_request;
  wc->size_allocate = gnome_applet_label_size_allocate;
  wc->map = gnome_applet_label_map;
  cc->forall = gnome_applet_label_forall;
  cc->child_type = gnome_applet_label_child_type;

  oc->get_property = gnome_applet_label_get_property;
  oc->set_property = gnome_applet_label_set_property;

  g_object_class_install_property (oc, PROP_TEXT,
    g_param_spec_string ("text", "Text", "The text that the label displays",
                         "", G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

void
gnome_applet_label_set_text (GnomeAppletLabel *gal, const char *text)
{
  GnomeAppletLabelPrivate *priv = GET_PRIVATE (gal);

  if (priv->text == NULL || strcmp (priv->text, text))
  {
    g_free (priv->text);
    priv->text = g_strdup (text);

    gtk_label_set_text (GTK_LABEL (priv->white), priv->text);
    gtk_label_set_text (GTK_LABEL (priv->black), priv->text);
  }
}

GtkWidget *
gnome_applet_label_new (const char *text)
{
  GnomeAppletLabel *label;

  label = g_object_new (GNOME_TYPE_APPLET_LABEL, "text", text, NULL);

  return GTK_WIDGET (label);
}

void
gnome_applet_label_set_sideways (GnomeAppletLabel *gal, gboolean sideways)
{
  GnomeAppletLabelPrivate *priv = GET_PRIVATE (gal);

  if (priv->sideways == sideways)
    return;

  priv->sideways = sideways;

  gtk_label_set_angle (GTK_LABEL (priv->black), 270. * sideways);
  gtk_label_set_angle (GTK_LABEL (priv->white), 270. * sideways);
}

int
gnome_applet_label_get_length (GnomeAppletLabel *gal)
{
  GnomeAppletLabelPrivate *priv = GET_PRIVATE (gal);
  return priv->length;
}

int
gnome_applet_label_get_thickness (GnomeAppletLabel *gal)
{
  GnomeAppletLabelPrivate *priv = GET_PRIVATE (gal);
  return priv->thickness;
}

 
